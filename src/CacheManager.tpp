#pragma once

#include "../include/CacheManager.h"

namespace CacheSys
{
    template <typename Key, typename Value>
    void CacheManager::createCache(const std::string &name, PolicyType policy, int capacity)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (caches_.count(name) > 0)
        {
            throw std::runtime_error("CacheManager: cache \"" + name + "\" already exists");
        }

        std::shared_ptr<CachePolicy<Key, Value>> cachePtr;
        std::string                              policyName;

        switch (policy)
        {
        case PolicyType::LRU:
            cachePtr   = std::make_shared<LruCache<Key, Value>>(capacity);
            policyName = "LRU";
            break;
        case PolicyType::LFU:
            cachePtr   = std::make_shared<LfuCache<Key, Value>>(capacity);
            policyName = "LFU";
            break;
        case PolicyType::ARC:
            cachePtr   = std::make_shared<ArcCache<Key, Value>>(static_cast<size_t>(capacity));
            policyName = "ARC";
            break;
        }

        ManagedCache entry;
        entry.cachePtr  = cachePtr;
        entry.getStats  = [cachePtr]() { return cachePtr->getStats(); };
        entry.policyName = policyName;
        entry.capacity  = capacity;

        caches_.emplace(name, std::move(entry));
        insertionOrder_.push_back(name);
    }

    template <typename Key, typename Value>
    std::shared_ptr<CachePolicy<Key, Value>> CacheManager::getCache(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = caches_.find(name);
        if (it == caches_.end())
        {
            throw std::runtime_error("CacheManager: cache \"" + name + "\" not found");
        }
        return std::any_cast<std::shared_ptr<CachePolicy<Key, Value>>>(it->second.cachePtr);
    }

    inline bool CacheManager::hasCache(const std::string &name) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return caches_.count(name) > 0;
    }

    inline void CacheManager::removeCache(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        caches_.erase(name);
        auto it = std::find(insertionOrder_.begin(), insertionOrder_.end(), name);
        if (it != insertionOrder_.end())
        {
            insertionOrder_.erase(it);
        }
    }

    inline std::vector<std::pair<std::string, CacheStats>> CacheManager::getAllStats() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::pair<std::string, CacheStats>> result;
        result.reserve(insertionOrder_.size());
        for (const auto &name : insertionOrder_)
        {
            auto it = caches_.find(name);
            if (it != caches_.end())
            {
                result.emplace_back(name, it->second.getStats());
            }
        }
        return result;
    }

    inline void CacheManager::printStats() const
    {
        // 单次加锁完成所有操作，避免与 getAllStats() 双重加锁死锁
        std::lock_guard<std::mutex> lock(mutex_);

        std::cout << "\n";
        std::cout << "╔═══════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                     CacheManager - 缓存统计报告                  ║\n";
        std::cout << "╠════════════════════╦════════╦══════════╦══════════╦══════════════╣\n";
        std::cout << "║ 名称               ║ 策略   ║  命中    ║  未命中  ║   命中率     ║\n";
        std::cout << "╠════════════════════╬════════╬══════════╬══════════╬══════════════╣\n";

        for (const auto &name : insertionOrder_)
        {
            auto cIt = caches_.find(name);
            if (cIt == caches_.end()) continue;

            const auto &entry = cIt->second;
            CacheStats   stats = entry.getStats();

            std::cout << "║ " << std::left  << std::setw(18) << name
                      << " ║ " << std::left  << std::setw(6)  << entry.policyName
                      << " ║ " << std::right << std::setw(8)  << stats.hits
                      << " ║ " << std::right << std::setw(8)  << stats.misses
                      << " ║ " << std::right << std::setw(10)
                      << std::fixed << std::setprecision(2) << (stats.hitRate() * 100.0) << "%"
                      << "  ║\n";
        }

        std::cout << "╚════════════════════╩════════╩══════════╩══════════╩══════════════╝\n";
        std::cout << "\n";
    }
}
