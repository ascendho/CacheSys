#pragma once

#include "../include/TtlCache.h"

namespace CacheSys
{
    template <typename Key, typename Value>
    TtlCache<Key, Value>::TtlCache(std::unique_ptr<CachePolicy<Key, Value>> inner,
                                   Duration defaultTtl)
        : inner_(std::move(inner)), defaultTtl_(defaultTtl)
    {
    }

    template <typename Key, typename Value>
    void TtlCache<Key, Value>::put(Key key, Value value)
    {
        put(key, value, defaultTtl_);
    }

    template <typename Key, typename Value>
    void TtlCache<Key, Value>::put(Key key, Value value, Duration ttl)
    {
        {
            std::lock_guard<std::mutex> lock(ttlMutex_);
            if (ttl > Duration::zero())
            {
                expiryMap_[key] = Clock::now() + ttl;
            }
            else
            {
                // ttl == 0 : 无过期时间，确保 expiryMap_ 中不存在旧记录
                expiryMap_.erase(key);
            }
        }
        inner_->put(key, value);
    }

    template <typename Key, typename Value>
    bool TtlCache<Key, Value>::get(Key key, Value &value)
    {
        // 先检查是否过期
        if (isExpiredAndRemove(key))
        {
            ++this->misses_;
            return false;
        }

        if (inner_->get(key, value))
        {
            ++this->hits_;
            return true;
        }

        ++this->misses_;
        return false;
    }

    template <typename Key, typename Value>
    Value TtlCache<Key, Value>::get(Key key)
    {
        Value value{};
        this->get(key, value);
        return value;
    }

    template <typename Key, typename Value>
    void TtlCache<Key, Value>::remove(Key key)
    {
        {
            std::lock_guard<std::mutex> lock(ttlMutex_);
            expiryMap_.erase(key);
        }
        inner_->remove(key);
    }

    template <typename Key, typename Value>
    void TtlCache<Key, Value>::purgeExpired()
    {
        std::lock_guard<std::mutex> lock(ttlMutex_);
        auto now = Clock::now();
        auto it  = expiryMap_.begin();
        while (it != expiryMap_.end())
        {
            if (it->second <= now)
            {
                inner_->remove(it->first);
                it = expiryMap_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    template <typename Key, typename Value>
    bool TtlCache<Key, Value>::isExpiredAndRemove(const Key &key)
    {
        std::lock_guard<std::mutex> lock(ttlMutex_);
        auto it = expiryMap_.find(key);
        if (it == expiryMap_.end())
        {
            return false;   // 无过期记录 → 未过期
        }
        if (it->second > Clock::now())
        {
            return false;   // 尚未到期
        }
        // 已过期：清理
        inner_->remove(key);
        expiryMap_.erase(it);
        return true;
    }
}
