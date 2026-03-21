#pragma once

#include "../include/LruCache.h"

namespace CacheSys
{
    // ===== LruNode 实现 =====

    // 构造函数：初始化键值对，访问计数默认1
    template <typename Key, typename Value>
    LruNode<Key, Value>::LruNode(Key key, Value value) : key_(key), value_(value), accessCount_(1) {}

    // 获取节点键
    template <typename Key, typename Value>
    Key LruNode<Key, Value>::getKey() const
    {
        return key_;
    }

    // 获取节点值
    template <typename Key, typename Value>
    Value LruNode<Key, Value>::getValue() const
    {
        return value_;
    }

    // 更新节点值
    template <typename Key, typename Value>
    void LruNode<Key, Value>::setValue(const Value &value)
    {
        value_ = value;
    }

    // 获取访问计数
    template <typename Key, typename Value>
    size_t LruNode<Key, Value>::getAccessCount() const
    {
        return accessCount_;
    }

    // 访问计数自增
    template <typename Key, typename Value>
    void LruNode<Key, Value>::incrementAccessCount()
    {
        ++accessCount_;
    }

    // ===== LruCache 实现 =====

    // 构造函数：初始化容量并创建哨兵链表
    template <typename Key, typename Value>
    LruCache<Key, Value>::LruCache(int capacity) : capacity_(capacity)
    {
        initializeList();
    }

    // 写入缓存：存在则更新，不存在则新增（满则淘汰）
    template <typename Key, typename Value>
    void LruCache<Key, Value>::put(Key key, Value value)
    {
        // 容量非法直接返回
        if (capacity_ <= 0)
        {
            return;
        }

        // 加锁保证线程安全
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it != nodeMap_.end())
        {
            // 命中：更新值并移到最近使用位置
            updateExistingNode(it->second, value);
            return;
        }

        // 未命中：淘汰（必要时）+ 新增节点
        addNewNode(key, value);
    }

    // 读取缓存：命中返回true并更新访问顺序，未命中更新统计
    template <typename Key, typename Value>
    bool LruCache<Key, Value>::get(Key key, Value &value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it == nodeMap_.end())
        {
            // 未命中：更新统计
            ++this->misses_;
            return false;
        }

        // 命中：刷新顺序 + 更新统计 + 返回值
        moveToMostRecent(it->second);
        value = it->second->getValue();
        ++this->hits_;
        return true;
    }

    // 便捷读取：封装带输出参数的get，未命中返回默认值
    template <typename Key, typename Value>
    Value LruCache<Key, Value>::get(Key key)
    {
        Value value{};
        this->get(key, value);
        return value;
    }

    // 删除指定key：从链表和哈希表中移除节点
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

    // 初始化哨兵链表：头尾哨兵简化边界处理
    template <typename Key, typename Value>
    void LruCache<Key, Value>::initializeList()
    {
        dummyHead_ = std::make_shared<LruNodeType>(Key(), Value());
        dummyTail_ = std::make_shared<LruNodeType>(Key(), Value());
        dummyHead_->next_ = dummyTail_;
        dummyTail_->prev_ = dummyHead_;
    }

    // 更新已存在节点：更新值 + 移到最近使用位置
    template <typename Key, typename Value>
    void LruCache<Key, Value>::updateExistingNode(NodePtr node, const Value &value)
    {
        node->setValue(value);
        moveToMostRecent(node);
    }

    // 新增节点：缓存满则淘汰最久未使用节点，再插入新节点
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

    // 移到最近使用位置：先摘除再插入尾部
    template <typename Key, typename Value>
    void LruCache<Key, Value>::moveToMostRecent(NodePtr node)
    {
        removeNode(node);
        insertNode(node);
    }

    // 从链表中摘除节点：处理前驱后继指针
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

    // 插入节点到链表尾部（最近使用位置）
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

    // 淘汰最久未使用节点：移除头哨兵后第一个节点
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
        ++this->evictions_;
    }

    // ===== LruKCache 实现 =====

    // 构造函数：初始化主缓存、历史缓存和K阈值
    template <typename Key, typename Value>
    LruKCache<Key, Value>::LruKCache(int capacity, int historyCapacity, int k)
        : LruCache<Key, Value>(capacity),
          k_(k),
          historyList_(std::make_unique<LruCache<Key, size_t>>(historyCapacity))
    {
    }

    // 读取缓存：先查主缓存，再累计历史计数，达标则移入主缓存
    template <typename Key, typename Value>
    Value LruKCache<Key, Value>::get(Key key)
    {
        std::lock_guard<std::mutex> lock(lruKMutex_);

        Value value{};
        bool inMainCache = LruCache<Key, Value>::get(key, value);

        // 累计历史访问次数
        size_t historyCount = historyList_->get(key);
        historyCount++;
        historyList_->put(key, historyCount);

        if (inMainCache)
        {
            return value;
        }

        // 达到K次阈值，将历史数据移入主缓存
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

    // 写入缓存：主缓存无则更新历史计数，达标则移入主缓存
    template <typename Key, typename Value>
    void LruKCache<Key, Value>::put(Key key, Value value)
    {
        std::lock_guard<std::mutex> lock(lruKMutex_);

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

    // ===== ShardedLruCache 实现 =====

    // 构造函数：初始化分片数量和每个分片的容量
    template <typename Key, typename Value>
    ShardedLruCache<Key, Value>::ShardedLruCache(size_t capacity, int sliceNum)
        : capacity_(capacity),
          sliceNum_(sliceNum > 0 ? sliceNum : static_cast<int>(std::thread::hardware_concurrency()))
    {
        if (sliceNum_ <= 0)
        {
            sliceNum_ = 1;
        }

        // 均分总容量到各分片
        size_t sliceSize = static_cast<size_t>(std::ceil(capacity_ / static_cast<double>(sliceNum_)));
        for (int i = 0; i < sliceNum_; ++i)
        {
            lruSliceCaches_.emplace_back(new LruCache<Key, Value>(static_cast<int>(sliceSize)));
        }
    }

    // 写入缓存：路由到对应分片
    template <typename Key, typename Value>
    void ShardedLruCache<Key, Value>::put(Key key, Value value)
    {
        size_t sliceIndex = hashKey(key) % static_cast<size_t>(sliceNum_);
        lruSliceCaches_[sliceIndex]->put(key, value);
    }

    // 读取缓存：路由到对应分片
    template <typename Key, typename Value>
    bool ShardedLruCache<Key, Value>::get(Key key, Value &value)
    {
        size_t sliceIndex = hashKey(key) % static_cast<size_t>(sliceNum_);
        return lruSliceCaches_[sliceIndex]->get(key, value);
    }

    // 便捷读取：路由到对应分片
    template <typename Key, typename Value>
    Value ShardedLruCache<Key, Value>::get(Key key)
    {
        Value value{};
        get(key, value);
        return value;
    }

    // 哈希函数：计算key对应的分片索引
    template <typename Key, typename Value>
    size_t ShardedLruCache<Key, Value>::hashKey(const Key &key) const
    {
        std::hash<Key> hashFunc;
        return hashFunc(key);
    }
}