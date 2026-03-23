#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

#include "CachePolicy.h"

namespace CacheSys
{
    /**
     * @class TtlCache
     * @brief 带有生存时间（TTL）限制的缓存装饰器
     * @tparam Key 键类型
     * @tparam Value 值类型
     * 
     * 该类包装了另一个缓存策略（inner_），并为其增加了过期时间管理。
     * 当尝试获取已过期的项时，它会自动从底层缓存中移除该项。
     */
    template <typename Key, typename Value>
    class TtlCache : public CachePolicy<Key, Value>
    {
    public:
        // 时间相关类型别名，使用 steady_clock 保证单调递增，不受系统时间修改影响
        using Duration  = std::chrono::milliseconds;
        using Clock     = std::chrono::steady_clock;
        using TimePoint = std::chrono::time_point<Clock>;

        /**
         * @brief 构造函数
         * @param inner 实际负责存储的底层缓存策略实例（如 LruCache 的智能指针）
         * @param defaultTtl 默认的生存时间。如果为 Duration::zero()，则默认不自动过期。
         */
        explicit TtlCache(std::unique_ptr<CachePolicy<Key, Value>> inner,
                          Duration defaultTtl = Duration::zero());

        /**
         * @brief 存入数据（使用构造时指定的默认 TTL）
         */
        void put(Key key, Value value) override;
        
        /**
         * @brief 存入数据并指定特定的生存时间
         * @param key 键
         * @param value 值
         * @param ttl 该项特有的生存时间
         */
        void put(Key key, Value value, Duration ttl);

        /**
         * @brief 获取数据（引用方式）
         * 内部会先检查数据是否过期，如果已过期则会将其从底层缓存移除。
         * @return bool 是否命中且未过期
         */
        bool get(Key key, Value &value) override;

        /**
         * @brief 获取数据（直接返回方式）
         * @return Value 命中的值，若未命中或已过期则返回默认值
         */
        Value get(Key key) override;

        /**
         * @brief 显式移除某个键
         */
        void remove(Key key) override;

        /**
         * @brief 扫描并清理所有已过期的缓存项
         * 这是一个主动维护函数，可以由后台线程定期调用，防止过期数据长期占用内存。
         */
        void purgeExpired();

    private:
        /**
         * @brief 检查指定键是否过期，若过期则执行移除操作
         * @param key 要检查的键
         * @return bool true 表示已过期且已被移除，false 表示未过期或不存在
         */
        bool isExpiredAndRemove(const Key &key);

        /// 被包装的底层缓存策略（例如 LruCache/LfuCache）
        std::unique_ptr<CachePolicy<Key, Value>> inner_;
        
        /// 默认的生存时间间隔
        Duration                                 defaultTtl_;
        
        /// 存储每个键对应的过期绝对时间点
        std::unordered_map<Key, TimePoint>       expiryMap_;   
        
        /// 保护 expiryMap_ 操作的互斥锁
        std::mutex                               ttlMutex_;
    };
}

// 包含模板的具体实现
#include "../src/TtlCache.tpp"