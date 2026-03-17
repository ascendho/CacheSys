#pragma once

// 缓存抽象接口与通用统计结构。
// 所有具体缓存策略（LRU/LFU/ARC/TTL 装饰器等）都基于这里统一对外暴露 put/get/remove 能力。

#include <atomic>
#include <cstddef>

namespace CacheSys
{
    // 缓存运行统计。
    // - hits: 命中次数
    // - misses: 未命中次数
    // - evictions: 淘汰次数
    struct CacheStats
    {
        size_t hits = 0;
        size_t misses = 0;
        size_t evictions = 0;

        // 返回命中率，范围 [0, 1]。
        double hitRate() const
        {
            size_t total = hits + misses;
            return total ? static_cast<double>(hits) / total : 0.0;
        }

        // 返回总请求数（命中 + 未命中）。
        size_t totalRequests() const { return hits + misses; }
    };

    // 缓存策略统一接口。
    // 这里不约束具体淘汰算法，只约束最基本的读写/删除行为以及统计能力。
    template <typename Key, typename Value>
    class CachePolicy
    {
    public:
        virtual ~CachePolicy() = default;

        // 写入或更新缓存项。
        virtual void put(Key key, Value value) = 0;

        // 读取缓存项。
        // 返回 true 表示命中，并通过 value 输出结果；返回 false 表示未命中。
        virtual bool get(Key key, Value &value) = 0;

        // 便捷读取接口：未命中时返回 Value 的默认构造值。
        virtual Value get(Key key) = 0;

        // 可选删除接口：默认空实现，具体策略按需覆盖。
        virtual void remove(Key key) {} // optional: subclasses may override

        // 获取当前缓存统计快照。
        CacheStats getStats() const
        {
            return {hits_.load(), misses_.load(), evictions_.load()};
        }

        // 重置统计值。
        void resetStats()
        {
            hits_ = 0;
            misses_ = 0;
            evictions_ = 0;
        }

    protected:
        // 采用原子计数，便于在并发场景下安全累积统计数据。
        mutable std::atomic<size_t> hits_{0};
        mutable std::atomic<size_t> misses_{0};
        mutable std::atomic<size_t> evictions_{0};
    };
}
