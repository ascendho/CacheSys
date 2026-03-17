#pragma once

#include <algorithm>
#include <any>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "ArcCache.h"
#include "CachePolicy.h"
#include "LfuCache.h"
#include "LruCache.h"

namespace CacheSys
{
    // CacheManager 统一管理多个命名缓存实例，支持：
    //   - 按名称创建/获取/删除缓存
    //   - 运行时选择底层策略（LRU / LFU / ARC）
    //   - 汇总打印所有缓存的命中率等统计信息
    class CacheManager
    {
    public:
        enum class PolicyType
        {
            LRU,
            LFU,
            ARC
        };

        // 创建一个命名缓存；若同名已存在则抛出 std::runtime_error
        template <typename Key, typename Value>
        void createCache(const std::string &name, PolicyType policy, int capacity);

        // 获取命名缓存的 shared_ptr；类型不匹配时 any_cast 抛出异常
        template <typename Key, typename Value>
        std::shared_ptr<CachePolicy<Key, Value>> getCache(const std::string &name);

        bool hasCache(const std::string &name) const;
        void removeCache(const std::string &name);

        // 打印所有缓存的统计摘要到 stdout
        void printStats() const;

        // 返回 (name, stats) 对的列表，顺序与创建顺序一致
        std::vector<std::pair<std::string, CacheStats>> getAllStats() const;

    private:
        struct ManagedCache
        {
            std::any                        cachePtr;   // shared_ptr<CachePolicy<K,V>>
            std::function<CacheStats()>     getStats;
            std::string                     policyName;
            int                             capacity;
        };

        mutable std::mutex                              mutex_;
        std::unordered_map<std::string, ManagedCache>   caches_;
        std::vector<std::string>                        insertionOrder_; // 保持创建顺序
    };
}

#include "../src/CacheManager.tpp"
