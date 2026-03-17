#pragma once

#include <atomic>
#include <cstddef>

namespace CacheSys
{
    struct CacheStats
    {
        size_t hits      = 0;
        size_t misses    = 0;
        size_t evictions = 0;

        double hitRate() const
        {
            size_t total = hits + misses;
            return total ? static_cast<double>(hits) / total : 0.0;
        }

        size_t totalRequests() const { return hits + misses; }
    };

    template <typename Key, typename Value>
    class CachePolicy
    {
    public:
        virtual ~CachePolicy() = default;

        virtual void put(Key key, Value value)  = 0;
        virtual bool get(Key key, Value &value) = 0;
        virtual Value get(Key key)              = 0;
        virtual void remove(Key key)            {}  // optional: subclasses may override

        CacheStats getStats() const
        {
            return {hits_.load(), misses_.load(), evictions_.load()};
        }

        void resetStats()
        {
            hits_      = 0;
            misses_    = 0;
            evictions_ = 0;
        }

    protected:
        mutable std::atomic<size_t> hits_{0};
        mutable std::atomic<size_t> misses_{0};
        mutable std::atomic<size_t> evictions_{0};
    };
}
