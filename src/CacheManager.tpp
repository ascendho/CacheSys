#pragma once

#include "../include/CacheManager.h"

namespace CacheSys
{
    /**
     * @brief 创建并注册一个新的缓存实例
     * @tparam Key 缓存键类型
     * @tparam Value 缓存值类型
     * @param name 缓存的唯一识别名
     * @param policy 淘汰策略（LRU/LFU/ARC）
     * @param capacity 缓存容量
     * 
     * 逻辑说明：
     * 1. 使用 lock_guard 确保创建过程线程安全。
     * 2. 检查重名和非法容量。
     * 3. 根据 policy 参数动态创建对应的子类实例（std::make_shared）。
     * 4. 关键点：将具体的模板实例包装进 ManagedCache 结构体，通过 std::any 抹除类型，
     *    并通过 Lambda 表达式捕获 getStats 方法，方便后续统一统计。
     */
    template <typename Key, typename Value>
    void CacheManager::createCache(const std::string &name, PolicyType policy, int capacity)
    {
        // 1. 加锁保护共享资源 caches_ 和 insertionOrder_
        std::lock_guard<std::mutex> lock(mutex_);

        // 2. 异常检查：防止同名缓存冲突
        if (caches_.count(name) > 0)
        {
            throw std::runtime_error("CacheManager: cache \"" + name + "\" already exists");
        }

        if (capacity <= 0)
        {
            throw std::invalid_argument("CacheManager: cache \"" + name + "\" capacity must be > 0");
        }

        // 3. 根据策略类型实例化具体的模板类
        std::shared_ptr<CachePolicy<Key, Value>> cachePtr;
        std::string policyName;

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
            // ARC 构造函数接收 size_t 类型容量
            cachePtr = std::make_shared<ArcCache<Key, Value>>(static_cast<size_t>(capacity));
            policyName = "ARC";
            break;
        }

        // 4. 封装到 ManagedCache 结构中
        ManagedCache entry;
        entry.cachePtr = cachePtr; // 存入 std::any，此时具体类型信息被抹除
        
        // 使用 Lambda 闭包捕获 cachePtr。
        // 这样即使以后不知道 Key/Value 是什么，也能通过调用这个 function 拿到统计数据。
        entry.getStats = [cachePtr]()
        { return cachePtr->getStats(); }; 
        
        entry.policyName = policyName;    
        entry.capacity = capacity;        

        // 5. 注册到管理映射表并记录插入顺序
        caches_.emplace(name, std::move(entry));
        insertionOrder_.push_back(name);
    }

    /**
     * @brief 获取指定名称的缓存指针
     * @tparam Key 期望的键类型
     * @tparam Value 期望的值类型
     * 
     * 注意：调用此函数时传入的 Key/Value 必须与 createCache 时一致，
     * 否则 std::any_cast 会抛出 bad_any_cast 异常。
     */
    template <typename Key, typename Value>
    std::shared_ptr<CachePolicy<Key, Value>> CacheManager::getCache(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = caches_.find(name);
        if (it == caches_.end())
        {
            throw std::runtime_error("CacheManager: cache \"" + name + "\" not found");
        }
        
        try
        {
            // 将抹除类型的 std::any 还原回原始的 shared_ptr 类型
            return std::any_cast<std::shared_ptr<CachePolicy<Key, Value>>>(it->second.cachePtr);
        }
        catch (const std::bad_any_cast &)
        {
            // 如果类型不匹配（例如存的是 int，却尝试按 string 取出），则报错
            throw std::runtime_error("CacheManager: cache \"" + name + "\" type mismatch");
        }
    }

    /** @brief 检查缓存是否存在（线程安全） */
    inline bool CacheManager::hasCache(const std::string &name) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return caches_.count(name) > 0;
    }

    /**
     * @brief 移除缓存
     * 同时从哈希表和顺序记录向量中清理
     */
    inline void CacheManager::removeCache(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 从映射表中删除（会自动销毁 ManagedCache 里的 shared_ptr，引用计数减一）
        caches_.erase(name);
        
        // 从顺序向量中删除
        auto it = std::find(insertionOrder_.begin(), insertionOrder_.end(), name);
        if (it != insertionOrder_.end())
        {
            insertionOrder_.erase(it);
        }
    }

    /**
     * @brief 获取所有受管缓存的统计快照
     * 返回一个包含名称和 Stats 结构体的向量
     */
    inline std::vector<std::pair<std::string, CacheStats>> CacheManager::getAllStats() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::pair<std::string, CacheStats>> result;
        result.reserve(insertionOrder_.size());

        // 按创建顺序遍历，调用存储的 Lambda 函数获取实时数据
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

    /**
     * @brief 打印漂亮的统计表格到标准输出
     * 使用 std::setw 和 std::left/right 控制对齐格式
     */
    inline void CacheManager::printStats() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // 打印表头
        std::cout << "\n";
        std::cout << "╔═══════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                     CacheManager - 缓存统计报告                  ║\n";
        std::cout << "╠════════════════════╦════════╦══════════╦══════════╦══════════════╣\n";
        std::cout << "║ 名称               ║ 策略   ║  命中    ║  未命中  ║   命中率     ║\n";
        std::cout << "╠════════════════════╬════════╬══════════╬══════════╬══════════════╣\n";

        // 遍历所有缓存并格式化输出每一行数据
        for (const auto &name : insertionOrder_)
        {
            auto cIt = caches_.find(name);
            if (cIt == caches_.end())
                continue;

            const auto &entry = cIt->second;
            CacheStats stats = entry.getStats(); // 调用 Lambda 获取数据

            std::cout << "║ " << std::left << std::setw(18) << name
                      << " ║ " << std::left << std::setw(6) << entry.policyName
                      << " ║ " << std::right << std::setw(8) << stats.hits
                      << " ║ " << std::right << std::setw(8) << stats.misses
                      << " ║ " << std::right << std::setw(10)
                      << std::fixed << std::setprecision(2) << (stats.hitRate() * 100.0) << "%"
                      << "  ║\n";
        }

        // 打印表尾
        std::cout << "╚════════════════════╩════════╩══════════╩══════════╩══════════════╝\n";
        std::cout << "\n";
    }
}