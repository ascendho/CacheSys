#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "ArcCache.h"
#include "CacheWithLoader.h"
#include "LfuCache.h"
#include "LruCache.h"
#include "TtlCache.h"

// CacheSys端到端测试集合：覆盖LRU/LFU/ARC、TTL与自动回源
namespace
{
    // ========== LRU 统计与基础行为 ==========

    // 验证命中/未命中统计与命中率计算
    TEST(CacheStatsSystemTest, LruStatsAreCounted)
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

    // 验证LRU淘汰策略与remove删除行为
    TEST(LruSystemTest, EvictionAndRemoveWork)
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

    // 验证LRU-K读取可触发晋升逻辑
    TEST(LruKSystemTest, ReadCanTriggerPromotion)
    {
        CacheSys::LruKCache<int, std::string> cache(2, 8, 2);

        cache.put(1, "A");
        EXPECT_EQ(cache.get(1), "A");

        cache.put(1, "A2");
        EXPECT_EQ(cache.get(1), "A2");
    }

    // 验证分片LRU能够跨分片存取数据
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

    // ========== LFU 行为 ==========

    // 验证LFU优先淘汰低频键
    TEST(LfuSystemTest, EvictsLeastFrequentlyUsed)
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

    // 验证purge可清空全部缓存数据
    TEST(LfuSystemTest, PurgeClearsEntries)
    {
        CacheSys::LfuCache<int, std::string> cache(2);
        std::string value;

        cache.put(1, "A");
        cache.put(2, "B");
        cache.purge();

        EXPECT_FALSE(cache.get(1, value));
        EXPECT_FALSE(cache.get(2, value));
    }

    // 验证分片LFU能够跨分片存取数据
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

    // ========== ARC 行为 ==========

    // 验证ARC基础put/get功能
    TEST(ArcSystemTest, PutAndGetBasic)
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

    // 验证ARC容量约束生效
    TEST(ArcSystemTest, CapacityIsRespected)
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

    // ========== TTL 缓存 ==========

    // 验证过期条目会转为未命中
    TEST(TtlSystemTest, ExpiredEntryBecomesMiss)
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

    // 验证ttl=0表示不过期
    TEST(TtlSystemTest, ZeroTtlMeansNoExpiration)
    {
        auto inner = std::make_unique<CacheSys::LruCache<int, std::string>>(4);
        CacheSys::TtlCache<int, std::string> cache(std::move(inner));
        std::string value;

        cache.put(1, "A");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        EXPECT_TRUE(cache.get(1, value));
        EXPECT_EQ(value, "A");
    }

    // 验证TTL缓存参数校验（空inner）
    TEST(TtlValidationTest, NullInnerThrows)
    {
        std::unique_ptr<CacheSys::CachePolicy<int, std::string>> empty;
        EXPECT_THROW(
            (CacheSys::TtlCache<int, std::string>(std::move(empty))),
            std::invalid_argument);
    }

    // ========== 自动回源缓存 ==========

    // 验证首次未命中触发加载、二次命中不再加载
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

    // 验证loader异常会向上传播，且调用次数准确累计
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

    // 验证CacheWithLoader参数校验（空inner/空loader）
    TEST(CacheWithLoaderValidationTest, NullInnerAndEmptyLoaderThrow)
    {
        std::unique_ptr<CacheSys::CachePolicy<int, std::string>> empty;
        EXPECT_THROW(
            (CacheSys::CacheWithLoader<int, std::string>(
                std::move(empty),
                [](const int &key) { return std::to_string(key); })),
            std::invalid_argument);

        auto inner = std::make_unique<CacheSys::LruCache<int, std::string>>(2);
        CacheSys::CacheWithLoader<int, std::string>::Loader emptyLoader;
        EXPECT_THROW(
            (CacheSys::CacheWithLoader<int, std::string>(std::move(inner), std::move(emptyLoader))),
            std::invalid_argument);
    }

}
