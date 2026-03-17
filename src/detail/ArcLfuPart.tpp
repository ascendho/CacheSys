#pragma once

namespace CacheSys
{
    template <typename Key, typename Value>
    ArcLfuPart<Key, Value>::ArcLfuPart(size_t capacity, size_t transformThreshold)
        : capacity_(capacity),
          ghostCapacity_(capacity),
          transformThreshold_(transformThreshold),
          minFreq_(0)
    {
        initializeLists();
    }

    template <typename Key, typename Value>
    bool ArcLfuPart<Key, Value>::put(Key key, Value value)
    {
        if (capacity_ == 0)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = mainCache_.find(key);
        if (it != mainCache_.end())
        {
            return updateExistingNode(it->second, value);
        }
        return addNewNode(key, value);
    }

    template <typename Key, typename Value>
    bool ArcLfuPart<Key, Value>::get(Key key, Value &value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = mainCache_.find(key);
        if (it == mainCache_.end())
        {
            return false;
        }

        updateNodeFrequency(it->second);
        value = it->second->getValue();
        return true;
    }

    template <typename Key, typename Value>
    bool ArcLfuPart<Key, Value>::contains(Key key)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return mainCache_.find(key) != mainCache_.end();
    }

    template <typename Key, typename Value>
    bool ArcLfuPart<Key, Value>::consumeGhostEntry(Key key)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = ghostCache_.find(key);
        if (it == ghostCache_.end())
        {
            return false;
        }

        removeFromList(it->second);
        ghostCache_.erase(it);
        return true;
    }

    template <typename Key, typename Value>
    void ArcLfuPart<Key, Value>::increaseCapacity()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++capacity_;
    }

    template <typename Key, typename Value>
    bool ArcLfuPart<Key, Value>::decreaseCapacity()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (capacity_ == 0)
        {
            return false;
        }

        if (mainCache_.size() >= capacity_)
        {
            evictLeastFrequent();
        }

        --capacity_;
        return true;
    }

    template <typename Key, typename Value>
    void ArcLfuPart<Key, Value>::initializeLists()
    {
        ghostHead_ = std::make_shared<NodeType>();
        ghostTail_ = std::make_shared<NodeType>();
        ghostHead_->next_ = ghostTail_;
        ghostTail_->prev_ = ghostHead_;
    }

    template <typename Key, typename Value>
    bool ArcLfuPart<Key, Value>::addNewNode(const Key &key, const Value &value)
    {
        if (mainCache_.size() >= capacity_)
        {
            evictLeastFrequent();
        }

        NodePtr newNode = std::make_shared<NodeType>(key, value);
        mainCache_[key] = newNode;

        freqMap_[1].push_back(newNode);
        minFreq_ = 1;
        return true;
    }

    template <typename Key, typename Value>
    bool ArcLfuPart<Key, Value>::updateExistingNode(NodePtr node, const Value &value)
    {
        node->setValue(value);
        updateNodeFrequency(node);
        return true;
    }

    template <typename Key, typename Value>
    void ArcLfuPart<Key, Value>::updateNodeFrequency(NodePtr node)
    {
        const size_t oldFreq = node->getAccessCount();
        node->incrementAccessCount();
        const size_t newFreq = node->getAccessCount();

        auto fIt = freqMap_.find(oldFreq);
        if (fIt != freqMap_.end())
        {
            fIt->second.remove(node);
            if (fIt->second.empty())
            {
                freqMap_.erase(fIt);
                if (oldFreq == minFreq_)
                {
                    minFreq_ = newFreq;
                }
            }
        }

        freqMap_[newFreq].push_back(node);
    }

    template <typename Key, typename Value>
    void ArcLfuPart<Key, Value>::evictLeastFrequent()
    {
        if (freqMap_.empty())
        {
            return;
        }

        auto fIt = freqMap_.find(minFreq_);
        if (fIt == freqMap_.end() || fIt->second.empty())
        {
            minFreq_ = freqMap_.begin()->first;
            fIt = freqMap_.find(minFreq_);
            if (fIt == freqMap_.end() || fIt->second.empty())
            {
                return;
            }
        }

        NodePtr victim = fIt->second.front();
        fIt->second.pop_front();

        if (fIt->second.empty())
        {
            freqMap_.erase(fIt);
            if (!freqMap_.empty())
            {
                minFreq_ = freqMap_.begin()->first;
            }
            else
            {
                minFreq_ = 0;
            }
        }

        mainCache_.erase(victim->getKey());

        if (ghostCache_.size() >= ghostCapacity_)
        {
            removeOldestGhost();
        }
        addToGhost(victim);
    }

    template <typename Key, typename Value>
    void ArcLfuPart<Key, Value>::removeFromList(NodePtr node)
    {
        if (!node || node->prev_.expired() || !node->next_)
        {
            return;
        }

        auto prev = node->prev_.lock();
        if (!prev)
        {
            return;
        }

        prev->next_ = node->next_;
        node->next_->prev_ = prev;
        node->next_ = nullptr;
    }

    template <typename Key, typename Value>
    void ArcLfuPart<Key, Value>::addToGhost(NodePtr node)
    {
        node->setAccessCount(1);
        node->next_ = ghostTail_;
        node->prev_ = ghostTail_->prev_;

        auto tailPrev = ghostTail_->prev_.lock();
        if (tailPrev)
        {
            tailPrev->next_ = node;
        }
        ghostTail_->prev_ = node;
        ghostCache_[node->getKey()] = node;
    }

    template <typename Key, typename Value>
    void ArcLfuPart<Key, Value>::removeOldestGhost()
    {
        NodePtr oldest = ghostHead_->next_;
        if (!oldest || oldest == ghostTail_)
        {
            return;
        }

        removeFromList(oldest);
        ghostCache_.erase(oldest->getKey());
    }
}
