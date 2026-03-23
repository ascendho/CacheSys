#include <chrono>
#include <filesystem>
#include <fstream>
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
#include "RuntimeConfig.h"
#include "StrategySelector.h"
#include "TtlCache.h"

// CacheSys端到端测试集合：覆盖LRU/LFU/ARC、TTL、自动回源、管理器、策略选择与运行时配置
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

    // ========== CacheManager ==========

    // 验证创建、获取、统计与删除流程
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

    // 验证重复名称创建会抛异常
    TEST(CacheManagerValidationTest, DuplicateNameThrows)
    {
        CacheSys::CacheManager manager;
        manager.createCache<int, std::string>("same", CacheSys::CacheManager::PolicyType::LRU, 2);
        EXPECT_THROW(
            (manager.createCache<int, std::string>("same", CacheSys::CacheManager::PolicyType::LFU, 2)),
            std::runtime_error);
    }

    // 验证非法容量会抛异常
    TEST(CacheManagerValidationTest, InvalidCapacityThrows)
    {
        CacheSys::CacheManager manager;
        EXPECT_THROW(
            (manager.createCache<int, std::string>("bad", CacheSys::CacheManager::PolicyType::ARC, -1)),
            std::invalid_argument);
    }

    // ========== StrategySelector ==========

    // 写多且热点变化大 -> 推荐LRU
    TEST(StrategySelectorTest, RecommendsLruForWriteHeavy)
    {
        auto rec = CacheSys::StrategySelector::recommend(64, 0.8, 0.2);
        EXPECT_EQ(rec.policy, CacheSys::CacheManager::PolicyType::LRU);
        EXPECT_EQ(rec.policyName, "LRU");
        EXPECT_FALSE(rec.reason.empty());
    }

    // 读多且热点稳定 -> 推荐LFU
    TEST(StrategySelectorTest, RecommendsLfuForStableReadHeavy)
    {
        auto rec = CacheSys::StrategySelector::recommend(256, 0.2, 0.9);
        EXPECT_EQ(rec.policy, CacheSys::CacheManager::PolicyType::LFU);
        EXPECT_EQ(rec.policyName, "LFU");
    }

    // 混合场景 -> 推荐ARC
    TEST(StrategySelectorTest, RecommendsArcForMixed)
    {
        auto rec = CacheSys::StrategySelector::recommend(128, 0.45, 0.55);
        EXPECT_EQ(rec.policy, CacheSys::CacheManager::PolicyType::ARC);
        EXPECT_EQ(rec.policyName, "ARC");
    }

    // ========== RuntimeConfig ==========

    // 验证配置解析、自动装配与基础功能联调
    TEST(RuntimeConfigTest, ParseAndAssembleFromFile)
    {
        namespace fs = std::filesystem;

        const fs::path configPath = fs::temp_directory_path() / "cachesys_runtime_test.conf"; // 临时配置文件路径
        {
            std::ofstream ofs(configPath.string());
            ASSERT_TRUE(ofs.good());
            ofs << "cache name=user-profile policy=LRU capacity=8 ttl_ms=0 loader=on\n";
            ofs << "cache name=session-store policy=ARC capacity=16 ttl_ms=50 loader=off\n";
        }

        auto cfg = CacheSys::RuntimeConfig::loadFromFile(configPath.string());
        ASSERT_EQ(cfg.instances.size(), 2U);
        EXPECT_EQ(cfg.instances[0].name, "user-profile");
        EXPECT_EQ(cfg.instances[1].policy, CacheSys::CacheManager::PolicyType::ARC);

        CacheSys::RuntimeAssembler::LoaderRegistry loaders;
        loaders["user-profile"] = [](const std::string &key)
        {
            return std::string("USER<") + key + ">";
        };

        auto caches = CacheSys::RuntimeAssembler::build(cfg, loaders);
        ASSERT_EQ(caches.size(), 2U);

        std::string value;
        bool first = caches.at("user-profile")->get("u1", value);
        bool second = caches.at("user-profile")->get("u1", value);
        EXPECT_FALSE(first);
        EXPECT_TRUE(second);
        EXPECT_EQ(value, "USER<u1>");

        caches.at("session-store")->put("s1", "session");
        EXPECT_TRUE(caches.at("session-store")->get("s1", value));
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        EXPECT_FALSE(caches.at("session-store")->get("s1", value));

        std::error_code ec; // 清理临时文件
        fs::remove(configPath, ec);
    }

    // 验证非法配置会抛出异常
    TEST(RuntimeConfigTest, InvalidConfigThrows)
    {
        namespace fs = std::filesystem;

        const fs::path badPath = fs::temp_directory_path() / "cachesys_runtime_bad.conf";
        {
            std::ofstream ofs(badPath.string());
            ASSERT_TRUE(ofs.good());
            ofs << "cache name=bad policy=UNKNOWN capacity=8 ttl_ms=0 loader=off\n";
        }

        EXPECT_THROW((CacheSys::RuntimeConfig::loadFromFile(badPath.string())), std::runtime_error);

        std::error_code ec; // 清理临时文件
        fs::remove(badPath, ec);
    }
}
