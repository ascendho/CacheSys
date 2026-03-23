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
    /**
     * @class CacheManager
     * @brief 缓存管理中心（单例或全局管理类）
     * 
     * 该类负责创建、查找、销毁和监控系统中的所有缓存实例。
     * 
     * 设计要点：
     * 1. **类型抹除 (Type Erasure)**：利用 std::any 存储不同 Key/Value 类型的模板缓存实例。
     * 2. **统一监控**：通过 std::function 捕获统计接口，无需知道具体的模板参数即可读取统计数据。
     * 3. **线程安全**：内部使用互斥锁保护缓存注册表。
     */
    class CacheManager
    {
    public:
        /**
         * @enum PolicyType
         * @brief 支持的缓存淘汰策略类型
         */
        enum class PolicyType
        {
            LRU, ///< 最近最少使用
            LFU, ///< 最不经常使用
            ARC  ///< 自适应替换缓存
        };

        /**
         * @brief 创建一个新的缓存实例并注册到管理器中
         * @tparam Key 缓存键类型
         * @tparam Value 缓存值类型
         * @param name 缓存的唯一标识名称
         * @param policy 采用的淘汰策略 (LRU/LFU/ARC)
         * @param capacity 缓存的最大容量
         * @throw std::runtime_error 如果名称已存在则抛出异常
         */
        template <typename Key, typename Value>
        void createCache(const std::string &name, PolicyType policy, int capacity);

        /**
         * @brief 根据名称获取缓存实例
         * @tparam Key 键类型（必须与创建时一致）
         * @tparam Value 值类型（必须与创建时一致）
         * @param name 缓存名称
         * @return std::shared_ptr<CachePolicy<Key, Value>> 缓存实例的智能指针
         * @throw std::bad_any_cast 如果尝试以错误的 Key/Value 类型获取缓存，或缓存不存在
         */
        template <typename Key, typename Value>
        std::shared_ptr<CachePolicy<Key, Value>> getCache(const std::string &name);

        /** @brief 检查是否存在指定名称的缓存 */
        bool hasCache(const std::string &name) const;

        /** @brief 移除并销毁指定名称的缓存 */
        void removeCache(const std::string &name);

        /**
         * @brief 在控制台以表格形式打印所有受管缓存的运行统计信息
         * 输出项包括：名称、策略、容量、命中次数、未命中次数、驱逐次数、命中率等。
         */
        void printStats() const;

        /**
         * @brief 获取所有缓存的统计数据快照
         * @return std::vector<std::pair<std::string, CacheStats>> 键值对列表（名称, 统计数据）
         */
        std::vector<std::pair<std::string, CacheStats>> getAllStats() const;

    private:
        /**
         * @struct ManagedCache
         * @brief 内部包装结构，用于存储被“抹除类型”后的缓存信息
         */
        struct ManagedCache
        {
            std::any                        cachePtr;   ///< 存储 std::shared_ptr<CachePolicy<K, V>>
            std::function<CacheStats()>     getStats;   ///< 闭包函数：用于在不通过模板的情况下读取统计数据
            std::string                     policyName; ///< 策略名称字符串（用于打印显示）
            int                             capacity;   ///< 缓存容量
        };

        mutable std::mutex                              mutex_;           ///< 保护 caches_ 的互斥锁
        std::unordered_map<std::string, ManagedCache>   caches_;          ///< 缓存注册表（名称 -> 包装后的对象）
        std::vector<std::string>                        insertionOrder_;  ///< 记录缓存创建顺序，用于格式化输出
    };
}

// 包含模板成员函数的具体实现
#include "../src/CacheManager.tpp"