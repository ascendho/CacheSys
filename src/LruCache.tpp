#pragma once

#include "../include/LruCache.h"

namespace CacheSys
{
    // ==========================================
    // LruNode 实现 - 缓存节点
    // ==========================================

    /**
     * @brief LruNode 构造函数
     * @param key 键
     * @param value 值
     * 初始访问次数设为 1
     */
    template <typename Key, typename Value>
    LruNode<Key, Value>::LruNode(Key key, Value value) : key_(key), value_(value), accessCount_(1) {}

    template <typename Key, typename Value>
    Key LruNode<Key, Value>::getKey() const { return key_; }

    template <typename Key, typename Value>
    Value LruNode<Key, Value>::getValue() const { return value_; }

    template <typename Key, typename Value>
    void LruNode<Key, Value>::setValue(const Value &value) { value_ = value; }

    template <typename Key, typename Value>
    size_t LruNode<Key, Value>::getAccessCount() const { return accessCount_; }

    template <typename Key, typename Value>
    void LruNode<Key, Value>::incrementAccessCount() { ++accessCount_; }

    // ==========================================
    // LruCache 实现 - 标准 LRU 策略
    // ==========================================

    template <typename Key, typename Value>
    LruCache<Key, Value>::LruCache(int capacity) : capacity_(capacity)
    {
        initializeList();
    }

    /**
     * @brief 插入或更新缓存项
     * 1. 检查容量。
     * 2. 加锁保证线程安全。
     * 3. 若键存在，更新值并移至末尾（最新访问）。
     * 4. 若不存在，检查是否满员，必要时淘汰旧节点，然后插入新节点。
     */
    template <typename Key, typename Value>
    void LruCache<Key, Value>::put(Key key, Value value)
    {
        if (capacity_ <= 0) return;

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);

        if (it != nodeMap_.end())
        {
            // 命中：更新值并移动到“最新使用”端
            updateExistingNode(it->second, value);
            return;
        }

        // 未命中：插入新节点
        addNewNode(key, value);
    }

    /**
     * @brief 获取缓存值并提升其优先级
     * 命中则增加 hits_ 计数，未命中增加 misses_ 计数。
     */
    template <typename Key, typename Value>
    bool LruCache<Key, Value>::get(Key key, Value &value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it == nodeMap_.end())
        {
            // 继承自 CachePolicy 的原子计数器
            ++this->misses_; 
            return false;
        }

        // 命中：移动到“最新使用”端
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

    /**
     * @brief 手动移除指定节点
     */
    template <typename Key, typename Value>
    void LruCache<Key, Value>::remove(Key key)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it == nodeMap_.end()) return;

        removeNode(it->second);
        nodeMap_.erase(it);
    }

    /**
     * @brief 初始化带有哨兵节点的双向链表
     * dummyHead_ 代表最久未访问端，dummyTail_ 代表最新访问端。
     * 使用哨兵节点可以避免在插入/删除时频繁判断指针是否为 nullptr。
     */
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
        // 如果达到容量上限，淘汰最久未使用的节点（靠近 Head 的节点）
        if (nodeMap_.size() >= static_cast<size_t>(capacity_))
        {
            evictLeastRecent();
        }

        NodePtr newNode = std::make_shared<LruNodeType>(key, value);

        // 插入到 Tail 之前
        insertNode(newNode); 
        nodeMap_[key] = newNode;
    }

    /**
     * @brief 将节点移至“最新”位置
     * 逻辑：先从当前位置断开，再重新插入到靠近 Tail 的位置。
     */
    template <typename Key, typename Value>
    void LruCache<Key, Value>::moveToMostRecent(NodePtr node)
    {
        removeNode(node);
        insertNode(node);
    }

    /**
     * @brief 从双向链表中移除节点（内部维护指针关系）
     */
    template <typename Key, typename Value>
    void LruCache<Key, Value>::removeNode(NodePtr node)
    {
        if (node->prev_.expired() || !node->next_) return;

        auto prev = node->prev_.lock();
        if (!prev) return;

        prev->next_ = node->next_;
        node->next_->prev_ = prev;
        
        // 清理节点自身指向，防止悬挂指针
        node->next_ = nullptr; 
    }

    /**
     * @brief 在链表末尾（dummyTail 之前）插入节点
     */
    template <typename Key, typename Value>
    void LruCache<Key, Value>::insertNode(NodePtr node)
    {
        node->next_ = dummyTail_;
        node->prev_ = dummyTail_->prev_;

        auto tailPrev = dummyTail_->prev_.lock();
        if (!tailPrev) return;

        tailPrev->next_ = node;
        dummyTail_->prev_ = node;
    }

    /**
     * @brief 驱逐最久未使用的节点
     * 逻辑：dummyHead_->next 指向的就是最老的数据。
     */
    template <typename Key, typename Value>
    void LruCache<Key, Value>::evictLeastRecent()
    {
        NodePtr leastRecent = dummyHead_->next_;
        if (!leastRecent || leastRecent == dummyTail_) return;

        removeNode(leastRecent);
        nodeMap_.erase(leastRecent->getKey());

        // 原子增加驱逐计数
        ++this->evictions_; 
    }

    // ==========================================
    // LruKCache 实现 - LRU-K 策略
    // ==========================================

    template <typename Key, typename Value>
    LruKCache<Key, Value>::LruKCache(int capacity, int historyCapacity, int k)
        : LruCache<Key, Value>(capacity),
          k_(k),
          historyList_(std::make_unique<LruCache<Key, size_t>>(historyCapacity))
    {
    }

    /**
     * @brief LRU-K 获取逻辑
     * 1. 尝试从正式缓存获取。
     * 2. 无论是否命中，都要更新其在历史队列（historyList_）中的访问次数。
     * 3. 如果不在正式缓存但访问次数达到 K，则从历史区晋升到正式缓存。
     */
    template <typename Key, typename Value>
    Value LruKCache<Key, Value>::get(Key key)
    {
        std::lock_guard<std::mutex> lock(lruKMutex_);

        Value value{};
        bool inMainCache = LruCache<Key, Value>::get(key, value);

        // 更新历史访问计数
        size_t historyCount = historyList_->get(key);
        historyCount++;
        historyList_->put(key, historyCount);

        if (inMainCache) return value;

        // 如果达到 K 次访问，尝试从历史记录中恢复数据并放入正式缓存
        if (historyCount >= static_cast<size_t>(k_))
        {
            auto it = historyValueMap_.find(key);
            if (it != historyValueMap_.end())
            {
                Value storedValue = it->second;
                historyList_->remove(key);                   // 从历史计数移除
                historyValueMap_.erase(it);                  // 从历史数据移除
                LruCache<Key, Value>::put(key, storedValue); // 进入正式 LRU
                return storedValue;
            }
        }
        
        return value;
    }

    template <typename Key, typename Value>
    void LruKCache<Key, Value>::put(Key key, Value value)
    {
        std::lock_guard<std::mutex> lock(lruKMutex_);

        Value existingValue{};
        bool inMainCache = LruCache<Key, Value>::get(key, existingValue);
        
        // 如果已经在正式缓存中，直接更新
        if (inMainCache)
        {
            LruCache<Key, Value>::put(key, value);
            return;
        }

        // 否则进入历史队列统计次数
        size_t historyCount = historyList_->get(key);
        historyCount++;
        historyList_->put(key, historyCount);
        historyValueMap_[key] = value;

        // 达到 K 次后晋升
        if (historyCount >= static_cast<size_t>(k_))
        {
            historyList_->remove(key);
            historyValueMap_.erase(key);
            LruCache<Key, Value>::put(key, value);
        }
    }

    // ==========================================
    // ShardedLruCache 实现 - 分片缓存（提高并发）
    // ==========================================

    template <typename Key, typename Value>
    ShardedLruCache<Key, Value>::ShardedLruCache(size_t capacity, int sliceNum)
        : capacity_(capacity),
          sliceNum_(sliceNum > 0 ? sliceNum : static_cast<int>(std::thread::hardware_concurrency()))
    {
        if (sliceNum_ <= 0) sliceNum_ = 1;

        // 计算每个分片的容量（总容量 / 分片数，向上取整）
        size_t sliceSize = static_cast<size_t>(std::ceil(capacity_ / static_cast<double>(sliceNum_)));
        for (int i = 0; i < sliceNum_; ++i)
        {
            lruSliceCaches_.emplace_back(new LruCache<Key, Value>(static_cast<int>(sliceSize)));
        }
    }

    /**
     * @brief 通过对 Key 哈希来确定将其放入哪个分片
     */
    template <typename Key, typename Value>
    void ShardedLruCache<Key, Value>::put(Key key, Value value)
    {
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

    /**
     * @brief 内部哈希函数，使用标准库的 std::hash
     */
    template <typename Key, typename Value>
    size_t ShardedLruCache<Key, Value>::hashKey(const Key &key) const
    {
        std::hash<Key> hashFunc;
        return hashFunc(key);
    }
}