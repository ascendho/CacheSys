#pragma once

#include <memory>

#include "CachePolicy.h"
#include "detail/ArcLfuPart.h"
#include "detail/ArcLruPart.h"

namespace CacheSys
{
    template <typename Key, typename Value>
    class ArcCache : public CachePolicy<Key, Value>
    {
    public:
        ArcCache(size_t capacity = 10, size_t transformThreshold = 2);
        ~ArcCache() override = default;

        void put(Key key, Value value) override;
        bool get(Key key, Value &value) override;
        Value get(Key key) override;

    private:
        bool checkGhostCaches(Key key);

    private:
        size_t capacity_;
        size_t transformThreshold_;
        std::unique_ptr<ArcLruPart<Key, Value>> lruPart_;
        std::unique_ptr<ArcLfuPart<Key, Value>> lfuPart_;
    };
}

#include "../src/ArcCache.tpp"
