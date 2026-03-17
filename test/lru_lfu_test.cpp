/*
 * 系统测试：
 * 1. cmake -S . -B build
 * 2. cmake --build build
 * 3. ctest --test-dir build --output-on-failure
 */

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "ArcCache.h"
#include "CacheManager.h"
#include "CacheWithLoader.h"
#include "LfuCache.h"
#include "LruCache.h"
#include "TtlCache.h"

namespace
{
    TEST(CachePolicyStatsTest, LruStatsAreCounted)
    {
        CacheSys::LruCache<int, std::string> cache(2);
        std::string value;

        cache.put(1, "A");
        EXPECT_TRUE(cache.get(1, value));
        EXPECT_FALSE(cache.get(2, value));

        auto stats = cache.getStats();
        EXPECT_EQ(stats.hits, 1U);
        EXPECT_EQ(stats.misses, 1U);
        EXPECT_DOUBLE_EQ(stats.hitRate(), 0.5);
    }

    TEST(LruCacheSystemTest, EvictionAndRemoveWork)
    {
        CacheSys::LruCache<int, std::string> cache(2);
        std::string value;

        cache.put(1, "A");
        cache.put(2, "B");
        EXPECT_TRUE(cache.get(1, value));

        cache.put(3, "C");
        EXPECT_FALSE(cache.get(2, value));
        EXPECT_TRUE(cache.get(1, value));
        EXPECT_TRUE(cache.get(3, value));

        cache.remove(1);
        EXPECT_FALSE(cache.get(1, value));
    }

    TEST(LruKSystemTest, ReadCanTriggerPromotion)
    {
        CacheSys::LruKCache<int, std::string> cache(2, 8, 2);

        cache.put(1, "A");
        EXPECT_EQ(cache.get(1), "A");

        cache.put(1, "A2");
        EXPECT_EQ(cache.get(1), "A2");
    }

    TEST(ShardedLruSystemTest, RoutesAndStoresAcrossShards)
    {
        CacheSys::ShardedLruCache<int, std::string> cache(12, 4);
        std::string value;

        for (int i = 0; i < 8; ++i)
        {
            cache.put(i, "V" + std::to_string(i));
        }

        for (int i = 0; i < 8; ++i)
        {
            EXPECT_TRUE(cache.get(i, value));
            EXPECT_EQ(value, "V" + std::to_string(i));
        }
    }

    TEST(LfuCacheSystemTest, EvictsLeastFrequentlyUsed)
    {
        CacheSys::LfuCache<int, std::string> cache(2);
        std::string value;

        cache.put(1, "A");
        cache.put(2, "B");
        EXPECT_TRUE(cache.get(1, value));

        cache.put(3, "C");

        EXPECT_TRUE(cache.get(1, value));
        EXPECT_FALSE(cache.get(2, value));
        EXPECT_TRUE(cache.get(3, value));
    }

    TEST(LfuCacheSystemTest, PurgeClearsEntries)
    {
        CacheSys::LfuCache<int, std::string> cache(2);
        std::string value;

        cache.put(1, "A");
        cache.put(2, "B");
        cache.purge();

        EXPECT_FALSE(cache.get(1, value));
        EXPECT_FALSE(cache.get(2, value));
    }

    TEST(ShardedLfuSystemTest, RoutesAndStoresAcrossShards)
    {
        CacheSys::ShardedLfuCache<int, std::string> cache(12, 3, 100);
        std::string value;

        for (int i = 0; i < 6; ++i)
        {
            cache.put(i, "L" + std::to_string(i));
        }

        for (int i = 0; i < 6; ++i)
        {
            EXPECT_TRUE(cache.get(i, value));
            EXPECT_EQ(value, "L" + std::to_string(i));
        }
    }

    TEST(ArcCacheSystemTest, PutAndGetBasic)
    {
        CacheSys::ArcCache<int, std::string> cache(2, 2);
        std::string value;

        cache.put(1, "A");
        cache.put(2, "B");

        EXPECT_TRUE(cache.get(1, value));
        EXPECT_EQ(value, "A");
        EXPECT_TRUE(cache.get(2, value));
        EXPECT_EQ(value, "B");
    }

    TEST(ArcCacheSystemTest, CapacityIsRespected)
    {
        CacheSys::ArcCache<int, std::string> cache(2, 2);
        std::string value;

        cache.put(1, "A");
        cache.put(2, "B");
        cache.put(3, "C");

        int hitCount = 0;
        hitCount += cache.get(1, value) ? 1 : 0;
        hitCount += cache.get(2, value) ? 1 : 0;
        hitCount += cache.get(3, value) ? 1 : 0;

        EXPECT_EQ(hitCount, 2);
    }

    TEST(TtlCacheSystemTest, ExpiredEntryBecomesMiss)
    {
        using namespace std::chrono_literals;

        auto inner = std::make_unique<CacheSys::LruCache<int, std::string>>(4);
        CacheSys::TtlCache<int, std::string> cache(std::move(inner));
        std::string value;

        cache.put(1, "A", 15ms);
        EXPECT_TRUE(cache.get(1, value));

        std::this_thread::sleep_for(25ms);
        EXPECT_FALSE(cache.get(1, value));
    }

    TEST(TtlCacheSystemTest, ZeroTtlMeansNoExpiration)
    {
        auto inner = std::make_unique<CacheSys::LruCache<int, std::string>>(4);
        CacheSys::TtlCache<int, std::string> cache(std::move(inner));
        std::string value;

        cache.put(1, "A");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        EXPECT_TRUE(cache.get(1, value));
        EXPECT_EQ(value, "A");
    }

    TEST(CacheWithLoaderSystemTest, MissLoadsThenHits)
    {
        auto inner = std::make_unique<CacheSys::LruCache<int, std::string>>(4);
        CacheSys::CacheWithLoader<int, std::string> cache(
            std::move(inner),
            [](const int &key) { return std::string("DB") + std::to_string(key); });

        std::string value;
        EXPECT_FALSE(cache.get(7, value));
        EXPECT_EQ(value, "DB7");
        EXPECT_EQ(cache.loaderCallCount(), 1U);

        EXPECT_TRUE(cache.get(7, value));
        EXPECT_EQ(value, "DB7");
        EXPECT_EQ(cache.loaderCallCount(), 1U);
    }

    TEST(CacheWithLoaderSystemTest, LoaderExceptionPropagates)
    {
        auto inner = std::make_unique<CacheSys::LruCache<int, std::string>>(2);
        CacheSys::CacheWithLoader<int, std::string> cache(
            std::move(inner),
            [](const int &) -> std::string { throw std::runtime_error("backend error"); });

        std::string value;
        EXPECT_THROW(cache.get(1, value), std::runtime_error);
        EXPECT_THROW(cache.get(1, value), std::runtime_error);
        EXPECT_EQ(cache.loaderCallCount(), 2U);
    }

    TEST(CacheManagerSystemTest, CreateGetAndRemoveCaches)
    {
        CacheSys::CacheManager manager;

        manager.createCache<int, std::string>("users", CacheSys::CacheManager::PolicyType::LRU, 3);
        manager.createCache<int, std::string>("orders", CacheSys::CacheManager::PolicyType::LFU, 3);
        manager.createCache<int, std::string>("events", CacheSys::CacheManager::PolicyType::ARC, 3);

        EXPECT_TRUE(manager.hasCache("users"));
        EXPECT_TRUE(manager.hasCache("orders"));
        EXPECT_TRUE(manager.hasCache("events"));

        auto users = manager.getCache<int, std::string>("users");
        std::string value;
        users->put(1, "Alice");
        EXPECT_TRUE(users->get(1, value));
        EXPECT_EQ(value, "Alice");

        auto allStats = manager.getAllStats();
        EXPECT_EQ(allStats.size(), 3U);

        manager.removeCache("orders");
        EXPECT_FALSE(manager.hasCache("orders"));
    }

    TEST(CacheManagerSystemTest, DuplicateNameThrows)
    {
        CacheSys::CacheManager manager;
        manager.createCache<int, std::string>("same", CacheSys::CacheManager::PolicyType::LRU, 2);
        EXPECT_THROW(
            (manager.createCache<int, std::string>("same", CacheSys::CacheManager::PolicyType::LFU, 2)),
            std::runtime_error);
    }
} // namespace
