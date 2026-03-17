#pragma once

#include "../include/LfuCache.h"

namespace CacheSys
{
    // ===== FreqList::Node =====

    template <typename Key, typename Value>
    FreqList<Key, Value>::Node::Node() : freq(1), next(nullptr) {}

    template <typename Key, typename Value>
    FreqList<Key, Value>::Node::Node(Key key, Value value) : freq(1), key(key), value(value), next(nullptr) {}

    // ===== FreqList =====

    template <typename Key, typename Value>
    FreqList<Key, Value>::FreqList(int n) : freq_(n)
    {
        // 哨兵头尾节点，简化插入/删除边界处理。
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

    template <typename Key, typename Value>
    void FreqList<Key, Value>::addNode(NodePtr node)
    {
        if (!node || !head_ || !tail_)
        {
            return;
        }

        // 插入到桶尾，表示同频率下较新访问。
        node->pre = tail_->pre;
        node->next = tail_;

        auto tailPre = tail_->pre.lock();
        if (!tailPre)
        {
            return;
        }

        tailPre->next = node;
        tail_->pre = node;
    }

    template <typename Key, typename Value>
    void FreqList<Key, Value>::removeNode(NodePtr node)
    {
        if (!node || !head_ || !tail_)
        {
            return;
        }
        if (node->pre.expired() || !node->next)
        {
            return;
        }

        auto pre = node->pre.lock();
        if (!pre)
        {
            return;
        }

        pre->next = node->next;
        node->next->pre = pre;
        node->next = nullptr;
    }

    template <typename Key, typename Value>
    typename FreqList<Key, Value>::NodePtr FreqList<Key, Value>::getFirstNode() const
    {
        // 返回桶内最老节点（用于同频率淘汰）。
        return head_->next;
    }

    // ===== LfuCache =====

    template <typename Key, typename Value>
    LfuCache<Key, Value>::LfuCache(int capacity, int maxAverageNum)
        : capacity_(capacity),
          minFreq_(std::numeric_limits<int>::max()),
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
        // 容量非法时直接忽略写入。
        if (capacity_ <= 0)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it != nodeMap_.end())
        {
            // 已存在：更新值并按命中路径提升频率。
            it->second->value = value;
            getInternal(it->second, value);
            return;
        }

        // 新 key：走插入路径。
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

        // 命中后要更新频率桶位置。
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

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::purge()
    {
        // 清空节点映射和频率桶，并重置统计辅助值。
        std::lock_guard<std::mutex> lock(mutex_);
        nodeMap_.clear();
        clearFreqLists();
        minFreq_ = 1;
        curAverageNum_ = 0;
        curTotalNum_ = 0;
    }

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::putInternal(Key key, Value value)
    {
        if (nodeMap_.size() >= static_cast<size_t>(capacity_))
        {
            // 满容量先淘汰。
            evictOneEntry();
        }

        NodePtr node = std::make_shared<Node>(key, value);
        nodeMap_[key] = node;
        addToFreqList(node);
        addFreqNum();
        // 新插入节点频率固定从 1 开始。
        minFreq_ = std::min(minFreq_, 1);
    }

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::getInternal(NodePtr node, Value &value)
    {
        // 读取并执行频率提升：旧桶删除 -> freq+1 -> 新桶插入。
        value = node->value;
        removeFromFreqList(node);
        node->freq++;
        addToFreqList(node);

        auto it = freqToFreqList_.find(minFreq_);
        if (node->freq - 1 == minFreq_ && it != freqToFreqList_.end() && it->second->isEmpty())
        {
            minFreq_++;
        }

        addFreqNum();
    }

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::evictOneEntry()
    {
        // 从当前最小频率桶中淘汰一个节点。
        auto it = freqToFreqList_.find(minFreq_);
        if (it == freqToFreqList_.end() || !it->second || it->second->isEmpty())
        {
            return;
        }

        NodePtr node = it->second->getFirstNode();
        if (!node || node == it->second->tail_)
        {
            return;
        }

        removeFromFreqList(node);
        nodeMap_.erase(node->key);
        decreaseFreqNum(node->freq);
        ++this->evictions_;
    }

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::removeFromFreqList(NodePtr node)
    {
        if (!node)
        {
            return;
        }

        auto freqIt = freqToFreqList_.find(node->freq);
        if (freqIt == freqToFreqList_.end() || !freqIt->second)
        {
            return;
        }

        freqIt->second->removeNode(node);
    }

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::addToFreqList(NodePtr node)
    {
        if (!node)
        {
            return;
        }

        auto freq = node->freq;
        auto freqIt = freqToFreqList_.find(freq);
        if (freqIt == freqToFreqList_.end())
        {
            // 首次出现该频率时动态创建桶。
            freqToFreqList_[freq] = new FreqList<Key, Value>(freq);
        }

        freqToFreqList_[freq]->addNode(node);
    }

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::addFreqNum()
    {
        // curTotalNum_ 近似描述全局频率规模，用于触发衰减。
        curTotalNum_++;
        if (nodeMap_.empty())
        {
            curAverageNum_ = 0;
        }
        else
        {
            curAverageNum_ = curTotalNum_ / static_cast<int>(nodeMap_.size());
        }

        if (curAverageNum_ > maxAverageNum_)
        {
            handleFrequencyOverflow();
        }
    }

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::decreaseFreqNum(int num)
    {
        curTotalNum_ -= num;
        if (curTotalNum_ < 0)
        {
            curTotalNum_ = 0;
        }

        if (nodeMap_.empty())
        {
            curAverageNum_ = 0;
        }
        else
        {
            curAverageNum_ = curTotalNum_ / static_cast<int>(nodeMap_.size());
        }
    }

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::handleFrequencyOverflow()
    {
        // 频率衰减：防止长期热点频率无限增长造成“历史绑架”。
        if (nodeMap_.empty())
        {
            return;
        }

        for (auto it = nodeMap_.begin(); it != nodeMap_.end(); ++it)
        {
            if (!it->second)
            {
                continue;
            }

            NodePtr node = it->second;
            removeFromFreqList(node);

            int oldFreq = node->freq;
            int decay = maxAverageNum_ / 2;
            node->freq -= decay;
            if (node->freq < 1)
            {
                node->freq = 1;
            }

            int delta = node->freq - oldFreq;
            curTotalNum_ += delta;
            addToFreqList(node);
        }

        if (curTotalNum_ < 0)
        {
            curTotalNum_ = 0;
        }
        updateMinFreq();
        if (nodeMap_.empty())
        {
            curAverageNum_ = 0;
        }
        else
        {
            curAverageNum_ = curTotalNum_ / static_cast<int>(nodeMap_.size());
        }
    }

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::updateMinFreq()
    {
        // 重新扫描所有非空桶，找到新的最小频率。
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
        // 释放通过 new 创建的频率桶，避免内存泄漏。
        for (auto &pair : freqToFreqList_)
        {
            delete pair.second;
            pair.second = nullptr;
        }
        freqToFreqList_.clear();
    }

    // ===== ShardedLfuCache =====

    template <typename Key, typename Value>
    ShardedLfuCache<Key, Value>::ShardedLfuCache(size_t capacity, int sliceNum, int maxAverageNum)
        : capacity_(capacity),
          sliceNum_(sliceNum > 0 ? sliceNum : static_cast<int>(std::thread::hardware_concurrency()))
    {
        if (sliceNum_ <= 0)
        {
            sliceNum_ = 1;
        }

        // 将总容量均匀拆分给各分片。
        size_t sliceSize = static_cast<size_t>(std::ceil(capacity_ / static_cast<double>(sliceNum_)));
        for (int i = 0; i < sliceNum_; ++i)
        {
            lfuSliceCaches_.emplace_back(new LfuCache<Key, Value>(static_cast<int>(sliceSize), maxAverageNum));
        }
    }

    template <typename Key, typename Value>
    void ShardedLfuCache<Key, Value>::put(Key key, Value value)
    {
        // 通过哈希路由到目标分片。
        size_t sliceIndex = hashKey(key) % static_cast<size_t>(sliceNum_);
        lfuSliceCaches_[sliceIndex]->put(key, value);
    }

    template <typename Key, typename Value>
    bool ShardedLfuCache<Key, Value>::get(Key key, Value &value)
    {
        size_t sliceIndex = hashKey(key) % static_cast<size_t>(sliceNum_);
        return lfuSliceCaches_[sliceIndex]->get(key, value);
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
