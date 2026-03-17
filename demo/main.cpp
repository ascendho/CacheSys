// CacheSys 端到端演示
// 展示缓存系统的完整工作流：CacheManager + CacheWithLoader + TtlCache + 统计报告
//
// 模拟场景：
//   - 用户服务：LRU 缓存 + 自动回源（CacheWithLoader）
//   - 商品目录：LFU 缓存 + 自动回源（访问频率差异大）
//   - 会话存储：LRU 缓存 + TTL 过期（TtlCache）


// cmake -S . -B build
// cmake --build build
// ./build/demo/cache_demo

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

#include "CacheManager.h"
#include "CacheWithLoader.h"
#include "LfuCache.h"
#include "LruCache.h"
#include "TtlCache.h"

using namespace CacheSys;
using namespace std::chrono_literals;

// ─── 工具函数 ────────────────────────────────────────────────────────────────

static void printSection(const std::string &title)
{
    std::cout << "\n┌─────────────────────────────────────────┐\n";
    std::cout << "│  " << std::left << std::setw(40) << title << "│\n";
    std::cout << "└─────────────────────────────────────────┘\n";
}

// ─── 模拟数据库 ──────────────────────────────────────────────────────────────

// 模拟用户数据库：记录实际查询次数
struct MockUserDB
{
    int queryCount = 0;

    std::string query(const std::string &userId)
    {
        ++queryCount;
        // 模拟 I/O 延迟（实际项目中可能耗时数十毫秒）
        std::cout << "    [DB] 查询用户 " << userId << " (第 " << queryCount << " 次 DB 访问)\n";
        return "User<" + userId + ">: name=张三_" + userId + ", role=admin";
    }
};

// 模拟商品数据库
struct MockProductDB
{
    int queryCount = 0;

    std::string query(int productId)
    {
        ++queryCount;
        std::cout << "    [DB] 查询商品 #" << productId << " (第 " << queryCount << " 次 DB 访问)\n";
        return "Product<" + std::to_string(productId) + ">: price=" +
               std::to_string(productId * 10) + "元, stock=100";
    }
};

// ─── 演示 1：CacheWithLoader（自动回源） ────────────────────────────────────

void demoAutoLoader()
{
    printSection("① CacheWithLoader - 缓存自动回源");

    MockUserDB db;

    // 将 LRU 缓存（容量 3）与 DB loader 绑定
    auto cache = CacheWithLoader<std::string, std::string>(
        std::make_unique<LruCache<std::string, std::string>>(3),
        [&db](const std::string &key) { return db.query(key); }
    );

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
    cache.get("u004");  // u001 → evicted
    std::cout << "    get(u001) 再次触发 DB 查询（已被淘汰）：\n";
    cache.get("u001");

    auto stats = cache.getStats();
    std::cout << "\n  统计：命中=" << stats.hits << "  未命中=" << stats.misses
              << "  命中率=" << std::fixed << std::setprecision(1)
              << (stats.hitRate() * 100.0) << "%"
              << "  实际 DB 查询=" << cache.loaderCallCount() << " 次\n";
}

// ─── 演示 2：TtlCache（TTL 过期） ────────────────────────────────────────────

void demoTtlCache()
{
    printSection("② TtlCache - 基于 TTL 的自动过期");

    // 底层使用 LRU，TTL 装饰层默认过期时间 300ms
    auto cache = TtlCache<std::string, std::string>(
        std::make_unique<LruCache<std::string, std::string>>(100),
        300ms   // 默认 TTL
    );

    std::cout << "\n  写入会话数据（TTL = 300ms）：\n";
    cache.put("session-A", "user=alice, expire_at=300ms");
    cache.put("session-B", "user=bob,   expire_at=300ms");
    // 为 session-C 单独设置更短的 TTL
    cache.put("session-C", "user=carol, expire_at=100ms", 100ms);

    std::cout << "    put session-A (TTL=300ms)\n";
    std::cout << "    put session-B (TTL=300ms)\n";
    std::cout << "    put session-C (TTL=100ms)\n";

    // 立即查询 → 全部命中
    std::cout << "\n  立即查询（应全部命中）：\n";
    for (const std::string &k : {"session-A", "session-B", "session-C"})
    {
        std::string v;
        bool hit = cache.get(k, v);
        std::cout << "    get(" << k << ") → " << (hit ? "HIT  " : "MISS ") << v << "\n";
    }

    // 等待 150ms，session-C 过期
    std::cout << "\n  等待 150ms（session-C 应已过期）...\n";
    std::this_thread::sleep_for(150ms);

    for (const std::string &k : {"session-A", "session-B", "session-C"})
    {
        std::string v;
        bool hit = cache.get(k, v);
        std::cout << "    get(" << k << ") → " << (hit ? "HIT  " : "MISS(已过期)") << "\n";
    }

    // 等待 200ms，session-A/B 也过期
    std::cout << "\n  再等待 200ms（session-A/B 也应过期）...\n";
    std::this_thread::sleep_for(200ms);

    cache.purgeExpired();   // 主动清理
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

// ─── 演示 3：CacheManager（多缓存统一管理） ───────────────────────────────────

void demoCacheManager()
{
    printSection("③ CacheManager - 多缓存实例统一管理");

    CacheManager manager;

    // 创建三个不同策略的命名缓存
    manager.createCache<std::string, std::string>("user-cache",    CacheManager::PolicyType::LRU, 50);
    manager.createCache<int, std::string>         ("product-cache", CacheManager::PolicyType::LFU, 200);
    manager.createCache<std::string, std::string>("config-cache",  CacheManager::PolicyType::ARC, 20);

    std::cout << "\n  已创建缓存：user-cache(LRU/50)  product-cache(LFU/200)  config-cache(ARC/20)\n";

    // 操作 user-cache
    {
        auto cache = manager.getCache<std::string, std::string>("user-cache");
        for (int i = 1; i <= 5; ++i)
            cache->put("user" + std::to_string(i), "data_" + std::to_string(i));
        std::string v;
        // 命中 3 次
        cache->get("user1", v); cache->get("user2", v); cache->get("user3", v);
        // 未命中 2 次（不存在）
        cache->get("user99", v); cache->get("user100", v);
    }

    // 操作 product-cache（模拟热点商品被频繁访问）
    {
        MockProductDB db;
        auto cache = manager.getCache<int, std::string>("product-cache");

        // 写入 10 个商品
        for (int id = 1; id <= 10; ++id)
            cache->put(id, db.query(id));
        // 热点商品 1、2、3 被访问多次
        std::string v;
        for (int round = 0; round < 5; ++round)
            for (int id : {1, 2, 3})
                cache->get(id, v);
        // 冷数据只访问一次
        for (int id : {7, 8, 9, 10})
            cache->get(id, v);

        std::cout << "\n  商品缓存：热点(1/2/3) x5 轮 + 冷数据(7~10) x1 轮\n";
        auto s = cache->getStats();
        std::cout << "  统计：命中=" << s.hits << "  未命中=" << s.misses
                  << "  命中率=" << std::fixed << std::setprecision(1)
                  << (s.hitRate() * 100.0) << "%\n";
    }

    // 操作 config-cache（ARC 自适应）
    {
        auto cache = manager.getCache<std::string, std::string>("config-cache");
        for (const std::string &k : {"db.host", "db.port", "db.name", "app.debug", "app.version"})
            cache->put(k, "value_" + k);
        std::string v;
        // 反复读取 → 应被 ARC LFU 部分识别为热点
        for (int i = 0; i < 4; ++i)
        {
            cache->get("db.host", v);
            cache->get("db.port", v);
        }
        cache->get("nonexist1", v);
        cache->get("nonexist2", v);
    }

    // 打印统一统计报告
    manager.printStats();
}

// ─── 演示 4：TtlCache + CacheWithLoader 组合使用 ─────────────────────────────

void demoComposed()
{
    printSection("④ 组合：TtlCache + CacheWithLoader（带过期的自动回源）");

    MockUserDB db;

    // 组合层次：TtlCache(200ms) → CacheWithLoader(自动回源) → LruCache
    auto ttlCache = TtlCache<std::string, std::string>(
        std::make_unique<CacheWithLoader<std::string, std::string>>(
            std::make_unique<LruCache<std::string, std::string>>(10),
            [&db](const std::string &k) { return db.query(k); }
        ),
        200ms
    );

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

// ─── main ────────────────────────────────────────────────────────────────────

int main()
{
    std::cout << "╔══════════════════════════════════════════════╗\n";
    std::cout << "║        CacheSys  端到端演示                  ║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n";

    demoAutoLoader();
    demoTtlCache();
    demoCacheManager();
    demoComposed();

    std::cout << "\n[完成] 所有演示结束。\n";
    return 0;
}
