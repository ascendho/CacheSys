#pragma once

#include "../include/LruCache.h"

namespace CacheSys
{
    // ===== LruNode =====

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

    // ===== LruCache =====

    template <typename Key, typename Value>
    LruCache<Key, Value>::LruCache(int capacity) : capacity_(capacity)
    {
        initializeList();
    }

    template <typename Key, typename Value>
    void LruCache<Key, Value>::put(Key key, Value value)
    {
        // 容量非法时直接忽略写入。
        if (capacity_ <= 0)
        {
            return;
        }

        // 整个 LRU 的核心状态由一把互斥锁保护。
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it != nodeMap_.end())
        {
            // 命中旧 key：更新值并刷新到“最近使用”。
            updateExistingNode(it->second, value);
            return;
        }

        // 未命中旧 key：必要时先淘汰，再插入新节点。
        addNewNode(key, value);
    }

    template <typename Key, typename Value>
    bool LruCache<Key, Value>::get(Key key, Value &value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it == nodeMap_.end())
        {
            // 未命中时只更新统计并返回。
            ++this->misses_;
            return false;
        }

        // 命中时刷新访问顺序，并返回数据。
        moveToMostRecent(it->second);
        value = it->second->getValue();
        ++this->hits_;
        return true;
    }

    template <typename Key, typename Value>
    Value LruCache<Key, Value>::get(Key key)
    {
        Value value{};
        this->get(key, value);
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
        // 使用首尾哨兵节点，避免插入/删除时处理大量边界分支。
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
            // 缓存已满时，淘汰链表头部的最久未使用节点。
            evictLeastRecent();
        }

        NodePtr newNode = std::make_shared<LruNodeType>(key, value);
        insertNode(newNode);
        nodeMap_[key] = newNode;
    }

    template <typename Key, typename Value>
    void LruCache<Key, Value>::moveToMostRecent(NodePtr node)
    {
        // 先摘除再插入尾部，保持双向链表顺序正确。
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
        // 约定链表尾部为“最近使用”位置。
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
        // 哨兵头节点的下一个有效节点就是最久未使用的数据。
        NodePtr leastRecent = dummyHead_->next_;
        if (!leastRecent || leastRecent == dummyTail_)
        {
            return;
        }

        removeNode(leastRecent);
        nodeMap_.erase(leastRecent->getKey());
        ++this->evictions_;
    }

    // ===== LruKCache =====

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
        // 先查主缓存；命中则说明已经是热点数据。
        bool inMainCache = LruCache<Key, Value>::get(key, value);

        // 无论主缓存是否命中，都累计历史访问次数。
        size_t historyCount = historyList_->get(key);
        historyCount++;
        historyList_->put(key, historyCount);

        if (inMainCache)
        {
            return value;
        }

        if (historyCount >= static_cast<size_t>(k_))
        {
            // 达到阈值后，将历史值提升到主缓存。
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

    // ===== ShardedLruCache =====

    template <typename Key, typename Value>
    ShardedLruCache<Key, Value>::ShardedLruCache(size_t capacity, int sliceNum)
        : capacity_(capacity),
          sliceNum_(sliceNum > 0 ? sliceNum : static_cast<int>(std::thread::hardware_concurrency()))
    {
        if (sliceNum_ <= 0)
        {
            sliceNum_ = 1;
        }

        // 将总容量均匀拆到各个分片上。
        size_t sliceSize = static_cast<size_t>(std::ceil(capacity_ / static_cast<double>(sliceNum_)));
        for (int i = 0; i < sliceNum_; ++i)
        {
            lruSliceCaches_.emplace_back(new LruCache<Key, Value>(static_cast<int>(sliceSize)));
        }
    }

    template <typename Key, typename Value>
    void ShardedLruCache<Key, Value>::put(Key key, Value value)
    {
        // 根据 key 的哈希把请求路由到指定分片。
        size_t sliceIndex = hashKey(key) % static_cast<size_t>(sliceNum_);
        lruSliceCaches_[sliceIndex]->put(key, value);
    }

    template <typename Key, typename Value>
    bool ShardedLruCache<Key, Value>::get(Key key, Value &value)
    {
        size_t sliceIndex = hashKey(key) % static_cast<size_t>(sliceNum_);
        return lruSliceCaches_[sliceIndex]->get(key, value);
    }

    template <typename Key, typename Value>
    Value ShardedLruCache<Key, Value>::get(Key key)
    {
        Value value{};
        get(key, value);
        return value;
    }

    template <typename Key, typename Value>
    size_t ShardedLruCache<Key, Value>::hashKey(const Key &key) const
    {
        std::hash<Key> hashFunc;
        return hashFunc(key);
    }
}
