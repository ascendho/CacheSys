#pragma once

#include "../include/CacheManager.h"

namespace CacheSys
{
    /**
     * @brief 创建指定策略的缓存实例并纳入管理
     * @tparam Key 缓存键类型（需与实际缓存策略匹配）
     * @tparam Value 缓存值类型（需与实际缓存策略匹配）
     * @param name 缓存实例的唯一名称（用于后续获取/删除）
     * @param policy 缓存策略类型（LRU/LFU/ARC）
     * @param capacity 缓存容量（最大可存储的键值对数量）
     * @throws std::runtime_error 若同名缓存已存在则抛异常
     * @details 核心逻辑：
     *  1. 加锁保证线程安全（多线程创建缓存无冲突）；
     *  2. 检查缓存名称唯一性，避免重复创建；
     *  3. 根据策略类型创建对应缓存实例（LRU/LFU/ARC）；
     *  4. 封装缓存实例到 ManagedCache 结构体（包含统计、策略名、容量等元信息）；
     *  5. 存入缓存管理容器，并记录创建顺序（保证统计报告顺序）。
     */
    template <typename Key, typename Value>
    void CacheManager::createCache(const std::string &name, PolicyType policy, int capacity)
    {
        // 加互斥锁（作用域结束自动解锁），保证多线程下创建缓存的原子性
        std::lock_guard<std::mutex> lock(mutex_);

        // 检查同名缓存是否已存在，存在则抛异常（避免覆盖）
        if (caches_.count(name) > 0)
        {
            throw std::runtime_error("CacheManager: cache \"" + name + "\" already exists");
        }

        if (capacity <= 0)
        {
            throw std::invalid_argument("CacheManager: cache \"" + name + "\" capacity must be > 0");
        }

        // 缓存实例指针（基类指针，支持多态）
        std::shared_ptr<CachePolicy<Key, Value>> cachePtr;
        // 策略名称（用于统计报告展示）
        std::string policyName;

        // 根据策略类型创建对应缓存实例
        switch (policy)
        {
        case PolicyType::LRU:
            cachePtr = std::make_shared<LruCache<Key, Value>>(capacity);
            policyName = "LRU";
            break;
        case PolicyType::LFU:
            cachePtr = std::make_shared<LfuCache<Key, Value>>(capacity);
            policyName = "LFU";
            break;
        case PolicyType::ARC:
            cachePtr = std::make_shared<ArcCache<Key, Value>>(static_cast<size_t>(capacity));
            policyName = "ARC";
            break;
        }

        // 封装缓存实例及元信息到 ManagedCache 结构体
        ManagedCache entry;
        entry.cachePtr = cachePtr; // 缓存实例（std::any 类型，支持任意Key/Value）
        entry.getStats = [cachePtr]()
        { return cachePtr->getStats(); }; // 统计获取函数（闭包）
        entry.policyName = policyName;    // 策略名称（用于展示）
        entry.capacity = capacity;        // 缓存容量（用于展示）

        // 将缓存实例存入管理容器（key=名称，value=ManagedCache）
        caches_.emplace(name, std::move(entry));
        // 记录创建顺序（保证统计报告按创建顺序输出）
        insertionOrder_.push_back(name);
    }

    /**
     * @brief 获取指定名称的缓存实例
     * @tparam Key 缓存键类型（需与创建时的类型一致，否则 any_cast 抛异常）
     * @tparam Value 缓存值类型（需与创建时的类型一致，否则 any_cast 抛异常）
     * @param name 缓存实例名称
     * @return 缓存实例的共享指针（基类指针，支持多态调用）
     * @throws std::runtime_error 若缓存不存在则抛异常
     * @details 核心逻辑：
     *  1. 加锁保证线程安全；
     *  2. 查找缓存名称，不存在则抛异常；
     *  3. 通过 std::any_cast 转换为指定类型的缓存指针（类型不匹配会抛 bad_any_cast）。
     */
    template <typename Key, typename Value>
    std::shared_ptr<CachePolicy<Key, Value>> CacheManager::getCache(const std::string &name)
    {
        // 加锁保证线程安全（多线程获取缓存无冲突）
        std::lock_guard<std::mutex> lock(mutex_);
        // 查找指定名称的缓存
        auto it = caches_.find(name);
        if (it == caches_.end())
        {
            // 缓存不存在，抛异常提示
            throw std::runtime_error("CacheManager: cache \"" + name + "\" not found");
        }
        // 将 std::any 类型的缓存指针转换为指定类型（类型不匹配会抛 std::bad_any_cast）
        try
        {
            return std::any_cast<std::shared_ptr<CachePolicy<Key, Value>>>(it->second.cachePtr);
        }
        catch (const std::bad_any_cast &)
        {
            throw std::runtime_error("CacheManager: cache \"" + name + "\" type mismatch");
        }
    }

    /**
     * @brief 检查指定名称的缓存是否存在
     * @param name 缓存实例名称
     * @return bool 存在返回true，否则false
     * @details 加锁保证线程安全，仅做存在性检查，无副作用
     */
    inline bool CacheManager::hasCache(const std::string &name) const
    {
        // 加锁保证线程安全
        std::lock_guard<std::mutex> lock(mutex_);
        // 检查缓存名称是否在管理容器中
        return caches_.count(name) > 0;
    }

    /**
     * @brief 删除指定名称的缓存实例
     * @param name 缓存实例名称
     * @details 核心逻辑：
     *  1. 加锁保证线程安全；
     *  2. 从管理容器中删除缓存实例；
     *  3. 从创建顺序列表中移除名称（保证统计报告一致性）。
     */
    inline void CacheManager::removeCache(const std::string &name)
    {
        // 加锁保证线程安全
        std::lock_guard<std::mutex> lock(mutex_);
        // 从管理容器中删除缓存
        caches_.erase(name);
        // 查找缓存名称在创建顺序列表中的位置
        auto it = std::find(insertionOrder_.begin(), insertionOrder_.end(), name);
        if (it != insertionOrder_.end())
        {
            // 从顺序列表中移除
            insertionOrder_.erase(it);
        }
    }

    /**
     * @brief 获取所有缓存实例的统计信息
     * @return 向量，每个元素是（缓存名称，统计信息）的键值对
     * @details 加锁保证线程安全，按创建顺序返回统计信息，用于自定义统计展示
     */
    inline std::vector<std::pair<std::string, CacheStats>> CacheManager::getAllStats() const
    {
        // 加锁保证线程安全
        std::lock_guard<std::mutex> lock(mutex_);
        // 预分配内存，提升性能
        std::vector<std::pair<std::string, CacheStats>> result;
        result.reserve(insertionOrder_.size());

        // 按创建顺序遍历所有缓存，收集统计信息
        for (const auto &name : insertionOrder_)
        {
            auto it = caches_.find(name);
            if (it != caches_.end())
            {
                // 调用缓存的统计函数，存入结果
                result.emplace_back(name, it->second.getStats());
            }
        }
        return result;
    }

    /**
     * @brief 打印格式化的缓存统计报告（核心可视化功能）
     * @details 核心逻辑：
     *  1. 单次加锁避免死锁（与 getAllStats 不双重加锁）；
     *  2. 按创建顺序遍历所有缓存，格式化输出名称、策略、命中/未命中、命中率；
     *  3. 表格化展示，直观清晰。
     */
    inline void CacheManager::printStats() const
    {
        // 单次加锁完成所有操作，避免与 getAllStats() 双重加锁导致死锁
        std::lock_guard<std::mutex> lock(mutex_);

        // 打印统计报告头部
        std::cout << "\n";
        std::cout << "╔═══════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                     CacheManager - 缓存统计报告                  ║\n";
        std::cout << "╠════════════════════╦════════╦══════════╦══════════╦══════════════╣\n";
        std::cout << "║ 名称               ║ 策略   ║  命中    ║  未命中  ║   命中率     ║\n";
        std::cout << "╠════════════════════╬════════╬══════════╬══════════╬══════════════╣\n";

        // 按创建顺序遍历所有缓存，输出统计信息
        for (const auto &name : insertionOrder_)
        {
            auto cIt = caches_.find(name);
            if (cIt == caches_.end())
                continue;

            const auto &entry = cIt->second;
            CacheStats stats = entry.getStats();

            // 格式化输出：名称（左对齐）、策略（左对齐）、命中/未命中（右对齐）、命中率（右对齐+百分比）
            std::cout << "║ " << std::left << std::setw(18) << name
                      << " ║ " << std::left << std::setw(6) << entry.policyName
                      << " ║ " << std::right << std::setw(8) << stats.hits
                      << " ║ " << std::right << std::setw(8) << stats.misses
                      << " ║ " << std::right << std::setw(10)
                      << std::fixed << std::setprecision(2) << (stats.hitRate() * 100.0) << "%"
                      << "  ║\n";
        }

        // 打印统计报告尾部
        std::cout << "╚════════════════════╩════════╩══════════╩══════════╩══════════════╝\n";
        std::cout << "\n";
    }
}