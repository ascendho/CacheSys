#pragma once

namespace CacheSys
{
    template <typename Key, typename Value>
    ArcLruPart<Key, Value>::ArcLruPart(size_t capacity, size_t transformThreshold)
        : capacity_(capacity), ghostCapacity_(capacity), transformThreshold_(transformThreshold)
    {
        initializeLists();
    }

    template <typename Key, typename Value>
    bool ArcLruPart<Key, Value>::put(Key key, Value value)
    {
        if (capacity_ == 0)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = mainCache_.find(key);
        if (it != mainCache_.end())
        {
            return updateNodeAccess(it->second, &value);
        }
        return addNewNode(key, value);
    }

    template <typename Key, typename Value>
    bool ArcLruPart<Key, Value>::get(Key key, Value &value, bool &shouldTransform)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = mainCache_.find(key);
        if (it == mainCache_.end())
        {
            return false;
        }

        updateNodeAccess(it->second, nullptr);
        value = it->second->getValue();
        shouldTransform = it->second->getAccessCount() >= transformThreshold_;
        return true;
    }

    template <typename Key, typename Value>
    bool ArcLruPart<Key, Value>::remove(Key key)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = mainCache_.find(key);
        if (it == mainCache_.end())
        {
            return false;
        }

        removeFromList(it->second);
        mainCache_.erase(it);
        return true;
    }

    template <typename Key, typename Value>
    bool ArcLruPart<Key, Value>::consumeGhostEntry(Key key)
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
    void ArcLruPart<Key, Value>::increaseCapacity()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++capacity_;
    }

    template <typename Key, typename Value>
    bool ArcLruPart<Key, Value>::decreaseCapacity()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (capacity_ == 0)
        {
            return false;
        }

        while (mainCache_.size() >= capacity_ && !mainCache_.empty())
        {
            evictLeastRecent();
        }

        --capacity_;
        return true;
    }

    template <typename Key, typename Value>
    void ArcLruPart<Key, Value>::initializeLists()
    {
        mainHead_ = std::make_shared<NodeType>();
        mainTail_ = std::make_shared<NodeType>();
        mainHead_->next_ = mainTail_;
        mainTail_->prev_ = mainHead_;

        ghostHead_ = std::make_shared<NodeType>();
        ghostTail_ = std::make_shared<NodeType>();
        ghostHead_->next_ = ghostTail_;
        ghostTail_->prev_ = ghostHead_;
    }

    template <typename Key, typename Value>
    bool ArcLruPart<Key, Value>::addNewNode(const Key &key, const Value &value)
    {
        if (mainCache_.size() >= capacity_)
        {
            evictLeastRecent();
        }

        NodePtr newNode = std::make_shared<NodeType>(key, value);
        mainCache_[key] = newNode;
        addToFront(newNode);
        return true;
    }

    template <typename Key, typename Value>
    bool ArcLruPart<Key, Value>::updateNodeAccess(NodePtr node, const Value *value)
    {
        if (value)
        {
            node->setValue(*value);
        }

        moveToFront(node);
        node->incrementAccessCount();
        return true;
    }

    template <typename Key, typename Value>
    void ArcLruPart<Key, Value>::moveToFront(NodePtr node)
    {
        removeFromList(node);
        addToFront(node);
    }

    template <typename Key, typename Value>
    void ArcLruPart<Key, Value>::addToFront(NodePtr node)
    {
        node->next_ = mainHead_->next_;
        node->prev_ = mainHead_;

        auto oldFirst = mainHead_->next_;
        if (oldFirst)
        {
            oldFirst->prev_ = node;
        }
        mainHead_->next_ = node;
    }

    template <typename Key, typename Value>
    void ArcLruPart<Key, Value>::removeFromList(NodePtr node)
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
    void ArcLruPart<Key, Value>::evictLeastRecent()
    {
        NodePtr victim = mainTail_->prev_.lock();
        if (!victim || victim == mainHead_)
        {
            return;
        }

        removeFromList(victim);
        mainCache_.erase(victim->getKey());

        if (ghostCache_.size() >= ghostCapacity_)
        {
            removeOldestGhost();
        }
        addToGhost(victim);
    }

    template <typename Key, typename Value>
    void ArcLruPart<Key, Value>::addToGhost(NodePtr node)
    {
        node->setAccessCount(1);
        node->next_ = ghostHead_->next_;
        node->prev_ = ghostHead_;

        auto oldFirst = ghostHead_->next_;
        if (oldFirst)
        {
            oldFirst->prev_ = node;
        }
        ghostHead_->next_ = node;
        ghostCache_[node->getKey()] = node;
    }

    template <typename Key, typename Value>
    void ArcLruPart<Key, Value>::removeOldestGhost()
    {
        NodePtr oldest = ghostTail_->prev_.lock();
        if (!oldest || oldest == ghostHead_)
        {
            return;
        }

        removeFromList(oldest);
        ghostCache_.erase(oldest->getKey());
    }
}
