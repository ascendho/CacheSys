#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "CachePolicy.h"

namespace CacheSys
{
    // TtlCache 为任意 CachePolicy 添加 TTL（过期时间）支持。
    // 采用装饰器模式：接收一个内部 CachePolicy，在其基础上叠加惰性过期检查。
    // - put(key, value)       使用默认 TTL（0 表示永不过期）
    // - put(key, value, ttl) 为该 key 指定独立 TTL
    // - get 时若 key 已过期，视为 miss 并延迟从内部缓存清除
    // - purgeExpired()        主动清除所有已过期的 key
    template <typename Key, typename Value>
    class TtlCache : public CachePolicy<Key, Value>
    {
    public:
        using Duration  = std::chrono::milliseconds;
        using Clock     = std::chrono::steady_clock;
        using TimePoint = std::chrono::time_point<Clock>;

        // inner      : 底层缓存策略（LRU/LFU/ARC 均可）
        // defaultTtl : 默认过期时长，0 表示永不过期
        explicit TtlCache(std::unique_ptr<CachePolicy<Key, Value>> inner,
                          Duration defaultTtl = Duration::zero());

        // 使用默认 TTL 写入
        void put(Key key, Value value) override;
        // 指定该条目的 TTL
        void put(Key key, Value value, Duration ttl);

        bool get(Key key, Value &value) override;
        Value get(Key key) override;
        void remove(Key key) override;

        // 主动扫描并清除所有已过期条目（含内部缓存）
        void purgeExpired();

    private:
        // 检查 key 是否已过期；若过期则从 expiryMap_ 和内部缓存移除，返回 true
        bool isExpiredAndRemove(const Key &key);

        std::unique_ptr<CachePolicy<Key, Value>> inner_;
        Duration                                 defaultTtl_;
        std::unordered_map<Key, TimePoint>       expiryMap_;   // key -> 过期时间点
        std::mutex                               ttlMutex_;
    };
}

#include "../src/TtlCache.tpp"
