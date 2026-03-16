/*
 * 单元测试：
 * 1. cmake -S . -B build
 * 2. cmake --build build
 * 3. ctest --test-dir build --output-on-failure
 *
 */

#include <string>

#include <gtest/gtest.h>

#include "LfuCache.h"
#include "LruCache.h"

namespace
{
    TEST(LruCacheTest, EvictsLeastRecentlyUsed)
    {
        CacheSys::LruCache<int, std::string> cache(2);
        cache.put(1, "A");
        cache.put(2, "B");

        std::string value;
        EXPECT_TRUE(cache.get(1, value));
        EXPECT_EQ(value, "A");

        cache.put(3, "C");

        EXPECT_FALSE(cache.get(2, value));
        EXPECT_TRUE(cache.get(1, value));
        EXPECT_EQ(value, "A");
        EXPECT_TRUE(cache.get(3, value));
        EXPECT_EQ(value, "C");
    }

    TEST(LruCacheTest, UpdateExistingKeyRefreshesRecency)
    {
        CacheSys::LruCache<int, std::string> cache(2);
        cache.put(1, "A");
        cache.put(2, "B");

        cache.put(1, "A2");
        cache.put(3, "C");

        std::string value;
        EXPECT_FALSE(cache.get(2, value));
        EXPECT_TRUE(cache.get(1, value));
        EXPECT_EQ(value, "A2");
        EXPECT_TRUE(cache.get(3, value));
        EXPECT_EQ(value, "C");
    }

    TEST(LruCacheTest, RemoveDeletesExistingKey)
    {
        CacheSys::LruCache<int, std::string> cache(2);
        cache.put(1, "A");

        cache.remove(1);

        std::string value;
        EXPECT_FALSE(cache.get(1, value));
    }

    TEST(LfuCacheTest, EvictsLeastFrequentlyUsed)
    {
        CacheSys::LfuCache<int, std::string> cache(2);
        cache.put(1, "A");
        cache.put(2, "B");

        std::string value;
        EXPECT_TRUE(cache.get(1, value));
        EXPECT_EQ(value, "A");

        cache.put(3, "C");

        EXPECT_TRUE(cache.get(1, value));
        EXPECT_EQ(value, "A");
        EXPECT_FALSE(cache.get(2, value));
        EXPECT_TRUE(cache.get(3, value));
        EXPECT_EQ(value, "C");
    }

    TEST(LfuCacheTest, TieBreaksByRecencyWithinSameFrequency)
    {
        CacheSys::LfuCache<int, std::string> cache(2);
        cache.put(1, "A");
        cache.put(2, "B");

        std::string value;
        EXPECT_TRUE(cache.get(1, value));
        EXPECT_TRUE(cache.get(2, value));

        cache.put(3, "C");

        EXPECT_FALSE(cache.get(1, value));
        EXPECT_TRUE(cache.get(2, value));
        EXPECT_EQ(value, "B");
        EXPECT_TRUE(cache.get(3, value));
        EXPECT_EQ(value, "C");
    }

    TEST(LfuCacheTest, PurgeClearsAllEntries)
    {
        CacheSys::LfuCache<int, std::string> cache(2);
        cache.put(1, "A");
        cache.put(2, "B");

        cache.purge();

        std::string value;
        EXPECT_FALSE(cache.get(1, value));
        EXPECT_FALSE(cache.get(2, value));
    }

    TEST(LfuCacheTest, ZeroCapacityStoresNothing)
    {
        CacheSys::LfuCache<int, std::string> cache(0);
        cache.put(1, "A");

        std::string value;
        EXPECT_FALSE(cache.get(1, value));
    }
} // namespace
