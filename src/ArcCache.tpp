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
        rebalanceByGhostHit(key);

        const bool inLfu = lfuPart_->contains(key);
        if (inLfu)
        {
            lfuPart_->put(key, value);
            // 防止同一个 key 同时驻留在 LRU/LFU 两侧造成容量折损。
            lruPart_->remove(key);
            return;
        }

        lruPart_->put(key, value);
    }

    template <typename Key, typename Value>
    bool ArcCache<Key, Value>::get(Key key, Value &value)
    {
        rebalanceByGhostHit(key);

        bool shouldTransform = false;
        if (lruPart_->get(key, value, shouldTransform))
        {
            if (shouldTransform)
            {
                lfuPart_->put(key, value);
                lruPart_->remove(key);
            }
            ++this->hits_;
            return true;
        }

        if (lfuPart_->get(key, value))
        {
            ++this->hits_;
            return true;
        }

        ++this->misses_;
        return false;
    }

    template <typename Key, typename Value>
    Value ArcCache<Key, Value>::get(Key key)
    {
        Value value{};
        this->get(key, value);
        return value;
    }

    template <typename Key, typename Value>
    bool ArcCache<Key, Value>::rebalanceByGhostHit(Key key)
    {
        bool inGhost = false;
        if (lruPart_->consumeGhostEntry(key))
        {
            if (lfuPart_->decreaseCapacity())
            {
                lruPart_->increaseCapacity();
            }
            inGhost = true;
        }
        else if (lfuPart_->consumeGhostEntry(key))
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
