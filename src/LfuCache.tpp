#pragma once

#include "../include/LfuCache.h"

namespace CacheSys
{
    // ==========================================
    // FreqList::Node 实现 - 频率链表节点
    // ==========================================

    template <typename Key, typename Value>
    FreqList<Key, Value>::Node::Node() : freq(1), next(nullptr) {}

    template <typename Key, typename Value>
    FreqList<Key, Value>::Node::Node(Key key, Value value) 
        : freq(1), key(key), value(value), next(nullptr) {}

    // ==========================================
    // FreqList 实现 - 同频节点双向链表
    // ==========================================

    template <typename Key, typename Value>
    FreqList<Key, Value>::FreqList(int n) : freq_(n)
    {
        // 初始化哨兵节点，简化插入和删除逻辑
        head_ = std::make_shared<Node>();
        tail_ = std::make_shared<Node>();
        head_->next = tail_;
        tail_->pre = head_;
    }

    template <typename Key, typename Value>
    bool FreqList<Key, Value>::isEmpty() const
    {
        return head_->next == tail_;
    }

    /**
     * @brief 将节点添加到当前频率链表的末尾（靠近 tail_）
     * 在该实现中，末尾代表该频率下最新访问，头部（head_->next）代表最旧访问。
     */
    template <typename Key, typename Value>
    void FreqList<Key, Value>::addNode(NodePtr node)
    {
        if (!node || !head_ || !tail_) return;

        node->pre = tail_->pre;
        node->next = tail_;

        auto tailPre = tail_->pre.lock();
        if (!tailPre) return;

        tailPre->next = node;
        tail_->pre = node;
    }

    /**
     * @brief 从双向链表中移除节点
     */
    template <typename Key, typename Value>
    void FreqList<Key, Value>::removeNode(NodePtr node)
    {
        if (!node || !head_ || !tail_) return;
        if (node->pre.expired() || !node->next) return;

        auto pre = node->pre.lock();
        if (!pre) return;

        pre->next = node->next;
        node->next->pre = pre;

        // 切断连接
        node->next = nullptr; 
    }

    /**
     * @brief 获取当前频率下最久未访问的节点（淘汰目标）
     */
    template <typename Key, typename Value>
    typename FreqList<Key, Value>::NodePtr FreqList<Key, Value>::getFirstNode() const
    {
        return head_->next;
    }

    // ==========================================
    // LfuCache 实现 - 核心逻辑
    // ==========================================

    template <typename Key, typename Value>
    LfuCache<Key, Value>::LfuCache(int capacity, int maxAverageNum)
        : capacity_(capacity),
          minFreq_(std::numeric_limits<int>::max()), // 初始设为无穷大
          maxAverageNum_(maxAverageNum),
          curAverageNum_(0),
          curTotalNum_(0)
    {
    }

    template <typename Key, typename Value>
    LfuCache<Key, Value>::~LfuCache()
    {
        clearFreqLists();
    }

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::put(Key key, Value value)
    {
        if (capacity_ <= 0) return;

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it != nodeMap_.end())
        {
            // 如果键已存在，更新其值，并增加访问频率
            it->second->value = value;
            getInternal(it->second, value);
            return;
        }

        // 插入新节点
        putInternal(key, value);
    }

    template <typename Key, typename Value>
    bool LfuCache<Key, Value>::get(Key key, Value &value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it == nodeMap_.end())
        {
            ++this->misses_;
            return false;
        }

        // 找到数据后，增加其频率
        getInternal(it->second, value);
        ++this->hits_;
        return true;
    }

    template <typename Key, typename Value>
    Value LfuCache<Key, Value>::get(Key key)
    {
        Value value{};
        this->get(key, value);
        return value;
    }

    /**
     * @brief 清空整个缓存
     */
    template <typename Key, typename Value>
    void LfuCache<Key, Value>::purge()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        nodeMap_.clear();
        clearFreqLists();
        minFreq_ = 1;
        curAverageNum_ = 0;
        curTotalNum_ = 0;
    }

    /**
     * @brief 内部插入逻辑：处理容量检查和淘汰
     */
    template <typename Key, typename Value>
    void LfuCache<Key, Value>::putInternal(Key key, Value value)
    {
        if (nodeMap_.size() >= static_cast<size_t>(capacity_))
        {
            evictOneEntry(); // 淘汰一个频率最低的项
        }

        NodePtr node = std::make_shared<Node>(key, value);
        nodeMap_[key] = node;
        addToFreqList(node); // 加入频率为 1 的桶
        addFreqNum();        // 更新平均频率统计
        minFreq_ = 1;        // 新插入数据的频率必定是 1
    }

    /**
     * @brief 内部获取逻辑：处理频率升级（移动桶）
     */
    template <typename Key, typename Value>
    void LfuCache<Key, Value>::getInternal(NodePtr node, Value &value)
    {
        const int oldFreq = node->freq;
        value = node->value;
        
        // 从当前频率桶移除，增加频率，加入新频率桶
        removeFromFreqList(node);
        node->freq++;
        addToFreqList(node);

        // 如果旧频率桶是当前的最小频率桶，且移除后变空了，需要更新全局最小频率
        auto it = freqToFreqList_.find(oldFreq);
        if (oldFreq == minFreq_ && (it == freqToFreqList_.end() || it->second->isEmpty()))
        {
            updateMinFreq();
        }

        addFreqNum();
    }

    /**
     * @brief 淘汰一个频率最低的数据项
     * 逻辑：去 minFreq_ 对应的链表里拿 head_->next（最旧的一个）
     */
    template <typename Key, typename Value>
    void LfuCache<Key, Value>::evictOneEntry()
    {
        auto it = freqToFreqList_.find(minFreq_);
        if (it == freqToFreqList_.end() || !it->second || it->second->isEmpty()) return;

        NodePtr node = it->second->getFirstNode(); // 获取最旧节点
        if (!node || node == it->second->tail_) return;

        removeFromFreqList(node);
        nodeMap_.erase(node->key);
        decreaseFreqNum(node->freq); // 更新频率统计

        // 重新计算 minFreq_
        if (!nodeMap_.empty())
        {
            updateMinFreq();
        }
        else
        {
            minFreq_ = 1;
        }

        ++this->evictions_;
    }

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::removeFromFreqList(NodePtr node)
    {
        if (!node) return;

        auto freqIt = freqToFreqList_.find(node->freq);
        if (freqIt == freqToFreqList_.end() || !freqIt->second) return;

        freqIt->second->removeNode(node);
        if (freqIt->second->isEmpty())
        {
            // 桶空了就删除，保持映射表整洁
            freqToFreqList_.erase(freqIt); 
        }
    }

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::addToFreqList(NodePtr node)
    {
        if (!node) return;

        auto freq = node->freq;
        auto freqIt = freqToFreqList_.find(freq);
        if (freqIt == freqToFreqList_.end())
        {
            // 如果该频率对应的桶还没创建，则创建它
            freqToFreqList_[freq] = std::make_unique<FreqList<Key, Value>>(freq);
        }

        freqToFreqList_[freq]->addNode(node);
    }

    /**
     * @brief 更新平均频率并检查是否需要“频率衰减（Aging）”
     * LFU 的一大弊端是某些数据在短时间内被高频访问后，即使不再使用也会长期霸占缓存。
     * 这里的 maxAverageNum_ 机制用于检测频率是否整体过高。
     */
    template <typename Key, typename Value>
    void LfuCache<Key, Value>::addFreqNum()
    {
        curTotalNum_++;
        curAverageNum_ = nodeMap_.empty() ? 0 : curTotalNum_ / static_cast<int>(nodeMap_.size());

        // 如果平均频率超过阈值，触发溢出处理逻辑
        if (curAverageNum_ > maxAverageNum_)
        {
            handleFrequencyOverflow();
        }
    }

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::decreaseFreqNum(int num)
    {
        curTotalNum_ -= num;
        if (curTotalNum_ < 0) curTotalNum_ = 0;
        curAverageNum_ = nodeMap_.empty() ? 0 : curTotalNum_ / static_cast<int>(nodeMap_.size());
    }

    /**
     * @brief 核心：频率衰减/溢出处理（老化机制）
     * 逻辑：当缓存整体频率过高时，人为将所有节点的频率减半。
     * 这使得新进的数据能有更多机会通过访问次数超越老旧的“僵尸”热点数据。
     */
    template <typename Key, typename Value>
    void LfuCache<Key, Value>::handleFrequencyOverflow()
    {
        if (nodeMap_.empty()) return;

        for (auto it = nodeMap_.begin(); it != nodeMap_.end(); ++it)
        {
            NodePtr node = it->second;
            if (!node) continue;

            removeFromFreqList(node);

            int oldFreq = node->freq;
            int decay = maxAverageNum_ / 2;     // 计算衰减值
            node->freq -= decay;
            if (node->freq < 1) node->freq = 1; // 最小频率不能低于 1

            int delta = node->freq - oldFreq;
            curTotalNum_ += delta;              // 更新总频率计数
            addToFreqList(node);                // 重新放入较低频率的桶
        }

        if (curTotalNum_ < 0) curTotalNum_ = 0;
        updateMinFreq();
        curAverageNum_ = nodeMap_.empty() ? 0 : curTotalNum_ / static_cast<int>(nodeMap_.size());
    }

    /**
     * @brief 遍历频率桶列表，找到当前存在的最小频率值
     */
    template <typename Key, typename Value>
    void LfuCache<Key, Value>::updateMinFreq()
    {
        minFreq_ = std::numeric_limits<int>::max();
        for (const auto &pair : freqToFreqList_)
        {
            if (pair.second && !pair.second->isEmpty())
            {
                minFreq_ = std::min(minFreq_, pair.first);
            }
        }

        if (minFreq_ == std::numeric_limits<int>::max())
        {
            minFreq_ = 1;
        }
    }

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::clearFreqLists()
    {
        freqToFreqList_.clear();
    }

    // ==========================================
    // ShardedLfuCache 实现 - 分片并发
    // ==========================================

    template <typename Key, typename Value>
    ShardedLfuCache<Key, Value>::ShardedLfuCache(size_t capacity, int sliceNum, int maxAverageNum)
        : capacity_(capacity),
          sliceNum_(sliceNum > 0 ? sliceNum : static_cast<int>(std::thread::hardware_concurrency()))
    {
        if (sliceNum_ <= 0) sliceNum_ = 1;

        size_t sliceSize = static_cast<size_t>(std::ceil(capacity_ / static_cast<double>(sliceNum_)));
        for (int i = 0; i < sliceNum_; ++i)
        {
            // 为每个分片初始化独立的 LfuCache
            lruSliceCaches_.emplace_back(new LfuCache<Key, Value>(static_cast<int>(sliceSize), maxAverageNum));
        }
    }

    template <typename Key, typename Value>
    void ShardedLfuCache<Key, Value>::put(Key key, Value value)
    {
        size_t sliceIndex = hashKey(key) % static_cast<size_t>(sliceNum_);
        lruSliceCaches_[sliceIndex]->put(key, value);
    }

    template <typename Key, typename Value>
    bool ShardedLfuCache<Key, Value>::get(Key key, Value &value)
    {
        size_t sliceIndex = hashKey(key) % static_cast<size_t>(sliceNum_);
        return lruSliceCaches_[sliceIndex]->get(key, value);
    }

    template <typename Key, typename Value>
    Value ShardedLfuCache<Key, Value>::get(Key key)
    {
        Value value{};
        get(key, value);
        return value;
    }

    template <typename Key, typename Value>
    void ShardedLfuCache<Key, Value>::purge()
    {
        for (auto &lfuSliceCache : lfuSliceCaches_)
        {
            lfuSliceCache->purge();
        }
    }

    template <typename Key, typename Value>
    size_t ShardedLfuCache<Key, Value>::hashKey(const Key &key) const
    {
        std::hash<Key> hashFunc;
        return hashFunc(key);
    }
}