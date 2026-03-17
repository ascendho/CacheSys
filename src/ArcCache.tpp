#pragma once

#include "../include/ArcCache.h"

namespace CacheSys
{
    template <typename Key, typename Value>
    ArcCache<Key, Value>::ArcCache(size_t capacity, size_t transformThreshold)
        : capacity_(capacity),
          transformThreshold_(transformThreshold),
          lruPart_(std::make_unique<ArcLruPart<Key, Value>>(capacity, transformThreshold)),
          lfuPart_(std::make_unique<ArcLfuPart<Key, Value>>(capacity, transformThreshold))
    {
    }

    template <typename Key, typename Value>
    void ArcCache<Key, Value>::put(Key key, Value value)
    {
        checkGhostCaches(key);

        const bool inLfu = lfuPart_->contain(key);
        lruPart_->put(key, value);
        if (inLfu)
        {
            lfuPart_->put(key, value);
        }
    }

    template <typename Key, typename Value>
    bool ArcCache<Key, Value>::get(Key key, Value &value)
    {
        checkGhostCaches(key);

        bool shouldTransform = false;
        if (lruPart_->get(key, value, shouldTransform))
        {
            if (shouldTransform)
            {
                lfuPart_->put(key, value);
            }
            ++hits_;
            return true;
        }

        if (lfuPart_->get(key, value))
        {
            ++hits_;
            return true;
        }

        ++misses_;
        return false;
    }

    template <typename Key, typename Value>
    Value ArcCache<Key, Value>::get(Key key)
    {
        Value value{};
        get(key, value);
        return value;
    }

    template <typename Key, typename Value>
    bool ArcCache<Key, Value>::checkGhostCaches(Key key)
    {
        bool inGhost = false;
        if (lruPart_->checkGhost(key))
        {
            if (lfuPart_->decreaseCapacity())
            {
                lruPart_->increaseCapacity();
            }
            inGhost = true;
        }
        else if (lfuPart_->checkGhost(key))
        {
            if (lruPart_->decreaseCapacity())
            {
                lfuPart_->increaseCapacity();
            }
            inGhost = true;
        }

        return inGhost;
    }
}
