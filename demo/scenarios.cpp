#include "scenarios.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "CacheManager.h"
#include "CacheWithLoader.h"
#include "LfuCache.h"
#include "LruCache.h"
#include "RuntimeConfig.h"
#include "StrategySelector.h"
#include "TtlCache.h"
#include "common.h"

using namespace CacheSys;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace
{
    fs::path resolveRuntimeConfigPath()
    {
        const std::vector<fs::path> candidates = {
            fs::path("demo") / "cache_runtime.conf",
            fs::path(".") / "cache_runtime.conf",
            fs::path("..") / "demo" / "cache_runtime.conf",
            fs::path("..") / ".." / "demo" / "cache_runtime.conf"
        };

        for (const auto &p : candidates)
        {
            if (fs::exists(p))
            {
                return p;
            }
        }

        throw std::runtime_error("Cannot find cache_runtime.conf in known locations");
    }

    void demoStrategySelector()
    {
        printSection("⓪ StrategySelector - 按负载特征推荐策略");

        struct Input
        {
            size_t capacity;
            double writeRatio;
            double stability;
            const char *label;
        };

        const Input samples[] = {
            {64, 0.75, 0.20, "写多+热点波动大"},
            {256, 0.15, 0.90, "读多+热点稳定"},
            {128, 0.45, 0.55, "混合负载"},
        };

        for (const auto &s : samples)
        {
            auto rec = StrategySelector::recommend(s.capacity, s.writeRatio, s.stability);
            std::cout << "\n  场景: " << s.label
                      << "\n    推荐策略: " << rec.policyName
                      << "\n    理由: " << rec.reason << "\n";
        }
    }

    void demoAutoLoader()
    {
        printSection("① CacheWithLoader - 缓存自动回源");

        MockUserDB db;
        auto cache = CacheWithLoader<std::string, std::string>(
            std::make_unique<LruCache<std::string, std::string>>(3),
            [&db](const std::string &key) { return db.query(key); });

        std::cout << "\n  第一轮访问（全部 cache miss，触发 DB 查询）：\n";
        for (const std::string &id : {"u001", "u002", "u003"})
        {
            std::string val = cache.get(id);
            std::cout << "    get(" << id << ") → " << val << "\n";
        }

        std::cout << "\n  第二轮访问（全部 cache hit，不触发 DB 查询）：\n";
        for (const std::string &id : {"u001", "u002", "u003"})
        {
            std::string val = cache.get(id);
            std::cout << "    get(" << id << ") → [已缓存]\n";
            (void)val;
        }

        std::cout << "\n  超出容量（u004 写入，u001 被淘汰）：\n";
        cache.get("u004");
        std::cout << "    get(u001) 再次触发 DB 查询（已被淘汰）：\n";
        cache.get("u001");

        auto stats = cache.getStats();
        std::cout << "\n  统计：命中=" << stats.hits << "  未命中=" << stats.misses
                  << "  命中率=" << std::fixed << std::setprecision(1)
                  << (stats.hitRate() * 100.0) << "%"
                  << "  实际 DB 查询=" << cache.loaderCallCount() << " 次\n";
    }

    void demoTtlCache()
    {
        printSection("② TtlCache - 基于 TTL 的自动过期");

        auto cache = TtlCache<std::string, std::string>(
            std::make_unique<LruCache<std::string, std::string>>(100),
            300ms);

        std::cout << "\n  写入会话数据（TTL = 300ms）：\n";
        cache.put("session-A", "user=alice, expire_at=300ms");
        cache.put("session-B", "user=bob,   expire_at=300ms");
        cache.put("session-C", "user=carol, expire_at=100ms", 100ms);

        std::cout << "    put session-A (TTL=300ms)\n";
        std::cout << "    put session-B (TTL=300ms)\n";
        std::cout << "    put session-C (TTL=100ms)\n";

        std::cout << "\n  立即查询（应全部命中）：\n";
        for (const std::string &k : {"session-A", "session-B", "session-C"})
        {
            std::string v;
            bool hit = cache.get(k, v);
            std::cout << "    get(" << k << ") → " << (hit ? "HIT  " : "MISS ") << v << "\n";
        }

        std::cout << "\n  等待 150ms（session-C 应已过期）...\n";
        std::this_thread::sleep_for(150ms);

        for (const std::string &k : {"session-A", "session-B", "session-C"})
        {
            std::string v;
            bool hit = cache.get(k, v);
            std::cout << "    get(" << k << ") → " << (hit ? "HIT  " : "MISS(已过期)") << "\n";
        }

        std::cout << "\n  再等待 200ms（session-A/B 也应过期）...\n";
        std::this_thread::sleep_for(200ms);

        cache.purgeExpired();
        for (const std::string &k : {"session-A", "session-B"})
        {
            std::string v;
            bool hit = cache.get(k, v);
            std::cout << "    get(" << k << ") → " << (hit ? "HIT  " : "MISS(已过期)") << "\n";
        }

        auto s = cache.getStats();
        std::cout << "\n  统计：命中=" << s.hits << "  未命中=" << s.misses
                  << "  命中率=" << std::fixed << std::setprecision(1)
                  << (s.hitRate() * 100.0) << "%\n";
    }

    void demoCacheManager()
    {
        printSection("③ CacheManager - 多缓存实例统一管理");

        CacheManager manager;
        manager.createCache<std::string, std::string>("user-cache", CacheManager::PolicyType::LRU, 50);
        manager.createCache<int, std::string>("product-cache", CacheManager::PolicyType::LFU, 200);
        manager.createCache<std::string, std::string>("config-cache", CacheManager::PolicyType::ARC, 20);

        std::cout << "\n  已创建缓存：user-cache(LRU/50)  product-cache(LFU/200)  config-cache(ARC/20)\n";

        {
            auto cache = manager.getCache<std::string, std::string>("user-cache");
            for (int i = 1; i <= 5; ++i)
            {
                cache->put("user" + std::to_string(i), "data_" + std::to_string(i));
            }
            std::string v;
            cache->get("user1", v);
            cache->get("user2", v);
            cache->get("user3", v);
            cache->get("user99", v);
            cache->get("user100", v);
        }

        {
            MockProductDB db;
            auto cache = manager.getCache<int, std::string>("product-cache");

            for (int id = 1; id <= 10; ++id)
            {
                cache->put(id, db.query(id));
            }

            std::string v;
            for (int round = 0; round < 5; ++round)
            {
                for (int id : {1, 2, 3})
                {
                    cache->get(id, v);
                }
            }
            for (int id : {7, 8, 9, 10})
            {
                cache->get(id, v);
            }

            std::cout << "\n  商品缓存：热点(1/2/3) x5 轮 + 冷数据(7~10) x1 轮\n";
            auto s = cache->getStats();
            std::cout << "  统计：命中=" << s.hits << "  未命中=" << s.misses
                      << "  命中率=" << std::fixed << std::setprecision(1)
                      << (s.hitRate() * 100.0) << "%\n";
        }

        {
            auto cache = manager.getCache<std::string, std::string>("config-cache");
            for (const std::string &k : {"db.host", "db.port", "db.name", "app.debug", "app.version"})
            {
                cache->put(k, "value_" + k);
            }

            std::string v;
            for (int i = 0; i < 4; ++i)
            {
                cache->get("db.host", v);
                cache->get("db.port", v);
            }
            cache->get("nonexist1", v);
            cache->get("nonexist2", v);
        }

        manager.printStats();
    }

    void demoComposed()
    {
        printSection("④ 组合：TtlCache + CacheWithLoader（带过期的自动回源）");

        MockUserDB db;

        auto ttlCache = TtlCache<std::string, std::string>(
            std::make_unique<CacheWithLoader<std::string, std::string>>(
                std::make_unique<LruCache<std::string, std::string>>(10),
                [&db](const std::string &k) { return db.query(k); }),
            200ms);

        std::cout << "\n  首次访问（cache miss，触发 DB 查询）：\n";
        for (const std::string &id : {"u001", "u002"})
        {
            std::string v = ttlCache.get(id);
            std::cout << "    get(" << id << ") → " << v << "\n";
        }

        std::cout << "  再次访问（cache hit，不触发 DB）：\n";
        for (const std::string &id : {"u001", "u002"})
        {
            std::string v = ttlCache.get(id);
            std::cout << "    get(" << id << ") → [已缓存]\n";
            (void)v;
        }

        std::cout << "  等待 250ms（TTL 过期）...\n";
        std::this_thread::sleep_for(250ms);

        std::cout << "  TTL 过期后再次访问（重新触发 DB 查询）：\n";
        for (const std::string &id : {"u001", "u002"})
        {
            std::string v = ttlCache.get(id);
            std::cout << "    get(" << id << ") → " << v << "\n";
        }

        auto s = ttlCache.getStats();
        std::cout << "\n  统计：命中=" << s.hits << "  未命中=" << s.misses
                  << "  命中率=" << std::fixed << std::setprecision(1)
                  << (s.hitRate() * 100.0) << "%\n";
    }

    void demoRuntimeConfig()
    {
        printSection("⑤ RuntimeConfig - 配置驱动自动装配");

        const fs::path configPath = resolveRuntimeConfigPath();
        std::cout << "\n  加载配置文件: " << configPath.string() << "\n";

        RuntimeConfig cfg = RuntimeConfig::loadFromFile(configPath.string());

        RuntimeAssembler::LoaderRegistry loaders;
        loaders["user-profile"] = [](const std::string &key)
        {
            return std::string("USER_DB<") + key + ">";
        };
        loaders["product-catalog"] = [](const std::string &key)
        {
            return std::string("PRODUCT_DB<") + key + ">";
        };

        auto caches = RuntimeAssembler::build(cfg, loaders);

        std::cout << "\n  已自动装配缓存实例:\n";
        for (const auto &item : cfg.instances)
        {
            std::cout << "    - " << item.name
                      << " policy=" << policyName(item.policy)
                      << " capacity=" << item.capacity
                      << " ttl_ms=" << item.ttlMs
                      << " loader=" << (item.enableLoader ? "on" : "off")
                      << "\n";
        }

        std::cout << "\n  运行一次简单读写验证:\n";
        {
            auto userCache = caches.at("user-profile");
            std::string value;
            bool hit1 = userCache->get("u100", value);
            bool hit2 = userCache->get("u100", value);
            std::cout << "    user-profile get(u100): first=" << (hit1 ? "HIT" : "MISS")
                      << ", second=" << (hit2 ? "HIT" : "MISS")
                      << ", value=" << value << "\n";
        }

        {
            auto sessionCache = caches.at("session-store");
            std::string value;
            sessionCache->put("s-1", "session-value");
            sessionCache->get("s-1", value);
            std::this_thread::sleep_for(350ms);
            bool hit = sessionCache->get("s-1", value);
            std::cout << "    session-store get(s-1) after 350ms: "
                      << (hit ? "HIT" : "MISS") << "\n";
        }
    }
} // namespace

void runAllDemos()
{
    std::cout << "╔══════════════════════════════════════════════╗\n";
    std::cout << "║        CacheSys  端到端演示                  ║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n";

    demoStrategySelector();
    demoAutoLoader();
    demoTtlCache();
    demoCacheManager();
    demoComposed();
    demoRuntimeConfig();

    std::cout << "\n[完成] 所有演示结束。\n";
}
