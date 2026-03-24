#include "scenarios.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "ArcCache.h"
#include "CacheWithLoader.h"
#include "LfuCache.h"
#include "LruCache.h"
#include "TtlCache.h"
#include "common.h"

using namespace CacheSys;
using namespace std::chrono_literals;

// 匿名命名空间：放置仅演示文件内部使用的辅助函数
namespace
{
    // 演示基础策略行为：相同访问序列下比较 LRU/LFU/ARC 的命中率
    void demoPolicyBasics()
    {
        printSection("⓪ 基础策略行为（LRU / LFU / ARC）");

        LruCache<int, std::string> lru(3);
        LfuCache<int, std::string> lfu(3);
        ArcCache<int, std::string> arc(3, 2);

        const std::vector<int> trace = {1, 2, 3, 1, 2, 4, 1, 3, 4, 1, 2, 5};
        std::string value;

        for (const int k : trace)
        {
            lru.put(k, "V" + std::to_string(k));
            lfu.put(k, "V" + std::to_string(k));
            arc.put(k, "V" + std::to_string(k));

            (void)lru.get(k, value);
            (void)lfu.get(k, value);
            (void)arc.get(k, value);
        }

        const auto s1 = lru.getStats();
        const auto s2 = lfu.getStats();
        const auto s3 = arc.getStats();

        std::cout << "\n  访问序列长度: " << trace.size() << "\n";
        std::cout << "  LRU: hit=" << s1.hits << ", miss=" << s1.misses
                  << ", hitRate=" << std::fixed << std::setprecision(1) << (s1.hitRate() * 100.0) << "%\n";
        std::cout << "  LFU: hit=" << s2.hits << ", miss=" << s2.misses
                  << ", hitRate=" << std::fixed << std::setprecision(1) << (s2.hitRate() * 100.0) << "%\n";
        std::cout << "  ARC: hit=" << s3.hits << ", miss=" << s3.misses
                  << ", hitRate=" << std::fixed << std::setprecision(1) << (s3.hitRate() * 100.0) << "%\n";
    }

    // 演示自动回源缓存：未命中自动调用loader并回填缓存
    void demoAutoLoader()
    {
        printSection("① CacheWithLoader - 缓存自动回源");

        MockUserDB db; // 模拟后端数据库

        // 基于LRU构建回源缓存
        auto cache = CacheWithLoader<std::string, std::string>(
            std::make_unique<LruCache<std::string, std::string>>(3),
            [&db](const std::string &key)
            { return db.query(key); });

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
            (void)val; // 避免未使用变量告警
        }

        std::cout << "\n  超出容量（u004 写入，u001 被淘汰）：\n";
        cache.get("u004");
        std::cout << "    get(u001) 再次触发 DB 查询（已被淘汰）：\n";
        cache.get("u001");

        // 打印命中/未命中与DB调用统计
        auto stats = cache.getStats();
        std::cout << "\n  统计：命中=" << stats.hits << "  未命中=" << stats.misses
                  << "  命中率=" << std::fixed << std::setprecision(1)
                  << (stats.hitRate() * 100.0) << "%"
                  << "  实际 DB 查询=" << cache.loaderCallCount() << " 次\n";
    }

    // 演示TTL缓存：展示过期前后命中变化
    void demoTtlCache()
    {
        printSection("② TtlCache - 基于 TTL 的自动过期");

        // 默认TTL=300ms
        auto cache = TtlCache<std::string, std::string>(
            std::make_unique<LruCache<std::string, std::string>>(100),
            300ms);

        std::cout << "\n  写入会话数据（TTL = 300ms）：\n";
        cache.put("session-A", "user=alice, expire_at=300ms");
        cache.put("session-B", "user=bob,   expire_at=300ms");
        cache.put("session-C", "user=carol, expire_at=100ms", 100ms); // 覆盖默认TTL

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

        // 等待一段时间，session-C应先过期
        std::cout << "\n  等待 150ms（session-C 应已过期）...\n";
        std::this_thread::sleep_for(150ms);

        for (const std::string &k : {"session-A", "session-B", "session-C"})
        {
            std::string v;
            bool hit = cache.get(k, v);
            std::cout << "    get(" << k << ") → " << (hit ? "HIT  " : "MISS(已过期)") << "\n";
        }

        // 再等待后，session-A/B也应过期
        std::cout << "\n  再等待 200ms（session-A/B 也应过期）...\n";
        std::this_thread::sleep_for(200ms);

        cache.purgeExpired(); // 主动清理过期项
        for (const std::string &k : {"session-A", "session-B"})
        {
            std::string v;
            bool hit = cache.get(k, v);
            std::cout << "    get(" << k << ") → " << (hit ? "HIT  " : "MISS(已过期)") << "\n";
        }

        // 输出统计
        auto s = cache.getStats();
        std::cout << "\n  统计：命中=" << s.hits << "  未命中=" << s.misses
                  << "  命中率=" << std::fixed << std::setprecision(1)
                  << (s.hitRate() * 100.0) << "%\n";
    }

    // 演示组合缓存：TTL + 自动回源
    void demoComposed()
    {
        printSection("④ 组合：TtlCache + CacheWithLoader（带过期的自动回源）");

        MockUserDB db;

        // 组合：LRU底座 + 回源加载 + TTL过期
        auto ttlCache = TtlCache<std::string, std::string>(
            std::make_unique<CacheWithLoader<std::string, std::string>>(
                std::make_unique<LruCache<std::string, std::string>>(10),
                [&db](const std::string &k)
                { return db.query(k); }),
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

        // TTL过期后再次访问将重新回源
        std::cout << "  等待 250ms（TTL 过期）...\n";
        std::this_thread::sleep_for(250ms);

        
        std::cout << "  TTL 过期后再次访问（重新触发 DB 查询）：\n";
        for (const std::string &id : {"u001", "u002"})
        {
            std::string v = ttlCache.get(id);
            std::cout << "    get(" << id << ") → " << v << "\n";
        }

        // 输出组合缓存统计
        auto s = ttlCache.getStats();
        std::cout << "\n  统计：命中=" << s.hits << "  未命中=" << s.misses
                  << "  命中率=" << std::fixed << std::setprecision(1)
                  << (s.hitRate() * 100.0) << "%\n";
    }
}

// 统一执行全部演示场景
void runAllDemos()
{
    std::cout << "╔══════════════════════════════════════════════╗\n";
    std::cout << "║        CacheSys  端到端演示                  ║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n";

    demoPolicyBasics(); // 基础策略行为
    demoAutoLoader();   // 自动回源
    demoTtlCache();     // TTL过期
    demoComposed();     // 组合缓存

    std::cout << "\n[完成] 所有演示结束。\n";
}