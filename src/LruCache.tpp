#pragma once

#include "../include/LruCache.h"

namespace CacheSys
{
    template <typename Key, typename Value>
    LruNode<Key, Value>::LruNode(Key key, Value value) : key_(key), value_(value), accessCount_(1) {}

    template <typename Key, typename Value>
    Key LruNode<Key, Value>::getKey() const
    {
        return key_;
    }

    template <typename Key, typename Value>
    Value LruNode<Key, Value>::getValue() const
    {
        return value_;
    }

    template <typename Key, typename Value>
    void LruNode<Key, Value>::setValue(const Value &value)
    {
        value_ = value;
    }

    template <typename Key, typename Value>
    size_t LruNode<Key, Value>::getAccessCount() const
    {
        return accessCount_;
    }

    template <typename Key, typename Value>
    void LruNode<Key, Value>::incrementAccessCount()
    {
        ++accessCount_;
    }

    template <typename Key, typename Value>
    LruCache<Key, Value>::LruCache(int capacity) : capacity_(capacity)
    {
        initializeList();
    }

    template <typename Key, typename Value>
    void LruCache<Key, Value>::put(Key key, Value value)
    {
        if (capacity_ <= 0)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it != nodeMap_.end())
        {
            updateExistingNode(it->second, value);
            return;
        }

        addNewNode(key, value);
    }

    template <typename Key, typename Value>
    bool LruCache<Key, Value>::get(Key key, Value &value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it == nodeMap_.end())
        {
            return false;
        }

        moveToMostRecent(it->second);
        value = it->second->getValue();
        return true;
    }

    template <typename Key, typename Value>
    Value LruCache<Key, Value>::get(Key key)
    {
        Value value{};
        get(key, value);
        return value;
    }

    template <typename Key, typename Value>
    void LruCache<Key, Value>::remove(Key key)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it == nodeMap_.end())
        {
            return;
        }

        removeNode(it->second);
        nodeMap_.erase(it);
    }

    template <typename Key, typename Value>
    void LruCache<Key, Value>::initializeList()
    {
        dummyHead_ = std::make_shared<LruNodeType>(Key(), Value());
        dummyTail_ = std::make_shared<LruNodeType>(Key(), Value());
        dummyHead_->next_ = dummyTail_;
        dummyTail_->prev_ = dummyHead_;
    }

    template <typename Key, typename Value>
    void LruCache<Key, Value>::updateExistingNode(NodePtr node, const Value &value)
    {
        node->setValue(value);
        moveToMostRecent(node);
    }

    template <typename Key, typename Value>
    void LruCache<Key, Value>::addNewNode(const Key &key, const Value &value)
    {
        if (nodeMap_.size() >= static_cast<size_t>(capacity_))
        {
            evictLeastRecent();
        }

        NodePtr newNode = std::make_shared<LruNodeType>(key, value);
        insertNode(newNode);
        nodeMap_[key] = newNode;
    }

    template <typename Key, typename Value>
    void LruCache<Key, Value>::moveToMostRecent(NodePtr node)
    {
        removeNode(node);
        insertNode(node);
    }

    template <typename Key, typename Value>
    void LruCache<Key, Value>::removeNode(NodePtr node)
    {
        if (node->prev_.expired() || !node->next_)
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
    void LruCache<Key, Value>::insertNode(NodePtr node)
    {
        node->next_ = dummyTail_;
        node->prev_ = dummyTail_->prev_;

        auto tailPrev = dummyTail_->prev_.lock();
        if (!tailPrev)
        {
            return;
        }

        tailPrev->next_ = node;
        dummyTail_->prev_ = node;
    }

    template <typename Key, typename Value>
    void LruCache<Key, Value>::evictLeastRecent()
    {
        NodePtr leastRecent = dummyHead_->next_;
        if (!leastRecent || leastRecent == dummyTail_)
        {
            return;
        }

        removeNode(leastRecent);
        nodeMap_.erase(leastRecent->getKey());
    }

    template <typename Key, typename Value>
    LruKCache<Key, Value>::LruKCache(int capacity, int historyCapacity, int k)
        : LruCache<Key, Value>(capacity),
          k_(k),
          historyList_(std::make_unique<LruCache<Key, size_t>>(historyCapacity))
    {
    }

    template <typename Key, typename Value>
    Value LruKCache<Key, Value>::get(Key key)
    {
        Value value{};
        bool inMainCache = LruCache<Key, Value>::get(key, value);

        size_t historyCount = historyList_->get(key);
        historyCount++;
        historyList_->put(key, historyCount);

        if (inMainCache)
        {
            return value;
        }

        if (historyCount >= static_cast<size_t>(k_))
        {
            auto it = historyValueMap_.find(key);
            if (it != historyValueMap_.end())
            {
                Value storedValue = it->second;
                historyList_->remove(key);
                historyValueMap_.erase(it);
                LruCache<Key, Value>::put(key, storedValue);
                return storedValue;
            }
        }

        return value;
    }

    template <typename Key, typename Value>
    void LruKCache<Key, Value>::put(Key key, Value value)
    {
        Value existingValue{};
        bool inMainCache = LruCache<Key, Value>::get(key, existingValue);
        if (inMainCache)
        {
            LruCache<Key, Value>::put(key, value);
            return;
        }

        size_t historyCount = historyList_->get(key);
        historyCount++;
        historyList_->put(key, historyCount);
        historyValueMap_[key] = value;

        if (historyCount >= static_cast<size_t>(k_))
        {
            historyList_->remove(key);
            historyValueMap_.erase(key);
            LruCache<Key, Value>::put(key, value);
        }
    }

    template <typename Key, typename Value>
    HashLruCaches<Key, Value>::HashLruCaches(size_t capacity, int sliceNum)
        : capacity_(capacity),
          sliceNum_(sliceNum > 0 ? sliceNum : static_cast<int>(std::thread::hardware_concurrency()))
    {
        if (sliceNum_ <= 0)
        {
            sliceNum_ = 1;
        }

        size_t sliceSize = static_cast<size_t>(std::ceil(capacity_ / static_cast<double>(sliceNum_)));
        for (int i = 0; i < sliceNum_; ++i)
        {
            lruSliceCaches_.emplace_back(new LruCache<Key, Value>(static_cast<int>(sliceSize)));
        }
    }

    template <typename Key, typename Value>
    void HashLruCaches<Key, Value>::put(Key key, Value value)
    {
        size_t sliceIndex = Hash(key) % static_cast<size_t>(sliceNum_);
        lruSliceCaches_[sliceIndex]->put(key, value);
    }

    template <typename Key, typename Value>
    bool HashLruCaches<Key, Value>::get(Key key, Value &value)
    {
        size_t sliceIndex = Hash(key) % static_cast<size_t>(sliceNum_);
        return lruSliceCaches_[sliceIndex]->get(key, value);
    }

    template <typename Key, typename Value>
    Value HashLruCaches<Key, Value>::get(Key key)
    {
        Value value{};
        get(key, value);
        return value;
    }

    template <typename Key, typename Value>
    size_t HashLruCaches<Key, Value>::Hash(Key key)
    {
        std::hash<Key> hashFunc;
        return hashFunc(key);
    }
}
