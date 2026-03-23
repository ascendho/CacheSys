#pragma once

#include "../include/TtlCache.h"

namespace CacheSys
{
    /**
     * @brief 构造函数
     * @param inner 实际存储数据的底层缓存实例（装饰器模式：包装现有的 LRU/LFU 等）
     * @param defaultTtl 默认生存时长
     * @throw std::invalid_argument 如果传入的底层缓存指针为空则抛出异常
     */
    template <typename Key, typename Value>
    TtlCache<Key, Value>::TtlCache(std::unique_ptr<CachePolicy<Key, Value>> inner,
                                   Duration defaultTtl)
        : inner_(std::move(inner)), defaultTtl_(defaultTtl)
    {
        if (!inner_)
        {
            throw std::invalid_argument("TtlCache: inner cache must not be null");
        }
    }

    /**
     * @brief 存入数据（使用默认 TTL）
     */
    template <typename Key, typename Value>
    void TtlCache<Key, Value>::put(Key key, Value value)
    {
        put(key, value, defaultTtl_);
    }

    /**
     * @brief 存入数据并指定生存时长
     * 逻辑：
     * 1. 计算绝对过期时间点 (当前时间 + TTL)。
     * 2. 更新 expiryMap_。如果 TTL 为 0，则从过期表中移除（表示永不过期）。
     * 3. 将实际数据存入底层 inner_ 缓存。
     */
    template <typename Key, typename Value>
    void TtlCache<Key, Value>::put(Key key, Value value, Duration ttl)
    {
        {
            // 锁定过期时间映射表，确保存储操作的线程安全
            std::lock_guard<std::mutex> lock(ttlMutex_);
            if (ttl > Duration::zero())
            {
                // 计算该键应该消失的绝对时间点
                expiryMap_[key] = Clock::now() + ttl;
            }
            else
            {
                // 如果 TTL <= 0，表示该项在 TtlCache 层级永不过期
                expiryMap_.erase(key);
            }
        }
        // 调用底层缓存进行物理存储
        inner_->put(key, value);
    }

    /**
     * @brief 获取数据（引用方式）
     * 逻辑：
     * 1. 惰性检查：首先检查数据是否已过期。
     * 2. 如果已过期，isExpiredAndRemove 会负责清理，此处返回 false (未命中)。
     * 3. 如果未过期，则尝试从底层缓存获取。
     */
    template <typename Key, typename Value>
    bool TtlCache<Key, Value>::get(Key key, Value &value)
    {
        // 步骤 1: 检查是否过期并尝试移除
        if (isExpiredAndRemove(key))
        {
            // 统计：虽然底层可能有，但时间上已失效，视为未命中
            ++this->misses_; 
            return false;
        }

        // 步骤 2: 从底层实际缓存获取
        if (inner_->get(key, value))
        {
            ++this->hits_;
            return true;
        }

        ++this->misses_;
        return false;
    }

    /**
     * @brief 获取数据（直接返回方式）
     */
    template <typename Key, typename Value>
    Value TtlCache<Key, Value>::get(Key key)
    {
        Value value{};
        this->get(key, value);
        return value;
    }

    /**
     * @brief 显式移除键
     * 同时清理过期时间映射表和底层物理缓存
     */
    template <typename Key, typename Value>
    void TtlCache<Key, Value>::remove(Key key)
    {
        {
            std::lock_guard<std::mutex> lock(ttlMutex_);
            expiryMap_.erase(key);
        }
        inner_->remove(key);
    }

    /**
     * @brief 主动清理所有已过期的项
     * 逻辑：遍历整个过期映射表，对比当前时间，剔除所有时间戳早于现在的项。
     * 建议由外部定时器或后台维护线程定期触发。
     */
    template <typename Key, typename Value>
    void TtlCache<Key, Value>::purgeExpired()
    {
        std::lock_guard<std::mutex> lock(ttlMutex_);
        auto now = Clock::now();
        auto it  = expiryMap_.begin();
        
        while (it != expiryMap_.end())
        {
            // 如果过期时间点早于或等于当前时间
            if (it->second <= now)
            {
                inner_->remove(it->first); // 从底层缓存移除
                it = expiryMap_.erase(it); // 从映射表中移除并获取下一个迭代器
            }
            else
            {
                ++it;
            }
        }
    }

    /**
     * @brief 内部辅助方法：判断是否过期并执行清理
     * @return true 如果项已过期并被成功移除，或者原本就不存在过期设置
     * @return false 如果项未过期
     */
    template <typename Key, typename Value>
    bool TtlCache<Key, Value>::isExpiredAndRemove(const Key &key)
    {
        std::lock_guard<std::mutex> lock(ttlMutex_);
        auto it = expiryMap_.find(key);
        
        // 如果不在过期映射表中，说明该项被视为永不过期（除非被 LRU 策略淘汰）
        if (it == expiryMap_.end())
        {
            return false;   
        }

        // 检查过期时间点是否在未来
        if (it->second > Clock::now())
        {
            return false; // 未过期
        }
        
        // 已过期：执行双重清理逻辑
        inner_->remove(key);   // 物理移除底层数据
        expiryMap_.erase(it);  // 移除时间记录
        return true;
    }
}