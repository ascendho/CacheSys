#pragma once

#include "../include/LfuCache.h"

namespace CacheSys
{
    template <typename Key, typename Value>
    FreqList<Key, Value>::Node::Node() : freq(1), next(nullptr) {}

    template <typename Key, typename Value>
    FreqList<Key, Value>::Node::Node(Key key, Value value) : freq(1), key(key), value(value), next(nullptr) {}

    template <typename Key, typename Value>
    FreqList<Key, Value>::FreqList(int n) : freq_(n)
    {
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
        return head_->next;
    }

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
        if (capacity_ <= 0)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it != nodeMap_.end())
        {
            it->second->value = value;
            getInternal(it->second, value);
            return;
        }

        putInternal(key, value);
    }

    template <typename Key, typename Value>
    bool LfuCache<Key, Value>::get(Key key, Value &value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it == nodeMap_.end())
        {
            return false;
        }

        getInternal(it->second, value);
        return true;
    }

    template <typename Key, typename Value>
    Value LfuCache<Key, Value>::get(Key key)
    {
        Value value{};
        get(key, value);
        return value;
    }

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

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::putInternal(Key key, Value value)
    {
        if (nodeMap_.size() >= static_cast<size_t>(capacity_))
        {
            kickOut();
        }

        NodePtr node = std::make_shared<Node>(key, value);
        nodeMap_[key] = node;
        addToFreqList(node);
        addFreqNum();
        minFreq_ = std::min(minFreq_, 1);
    }

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::getInternal(NodePtr node, Value &value)
    {
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
    void LfuCache<Key, Value>::kickOut()
    {
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
            freqToFreqList_[freq] = new FreqList<Key, Value>(freq);
        }

        freqToFreqList_[freq]->addNode(node);
    }

    template <typename Key, typename Value>
    void LfuCache<Key, Value>::addFreqNum()
    {
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
            HandleOverMaxAverageNum();
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
    void LfuCache<Key, Value>::HandleOverMaxAverageNum()
    {
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
        for (auto &pair : freqToFreqList_)
        {
            delete pair.second;
            pair.second = nullptr;
        }
        freqToFreqList_.clear();
    }

    template <typename Key, typename Value>
    HashLfuCache<Key, Value>::HashLfuCache(size_t capacity, int sliceNum, int maxAverageNum)
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
            lfuSliceCaches_.emplace_back(new LfuCache<Key, Value>(static_cast<int>(sliceSize), maxAverageNum));
        }
    }

    template <typename Key, typename Value>
    void HashLfuCache<Key, Value>::put(Key key, Value value)
    {
        size_t sliceIndex = Hash(key) % static_cast<size_t>(sliceNum_);
        lfuSliceCaches_[sliceIndex]->put(key, value);
    }

    template <typename Key, typename Value>
    bool HashLfuCache<Key, Value>::get(Key key, Value &value)
    {
        size_t sliceIndex = Hash(key) % static_cast<size_t>(sliceNum_);
        return lfuSliceCaches_[sliceIndex]->get(key, value);
    }

    template <typename Key, typename Value>
    Value HashLfuCache<Key, Value>::get(Key key)
    {
        Value value{};
        get(key, value);
        return value;
    }

    template <typename Key, typename Value>
    void HashLfuCache<Key, Value>::purge()
    {
        for (auto &lfuSliceCache : lfuSliceCaches_)
        {
            lfuSliceCache->purge();
        }
    }

    template <typename Key, typename Value>
    size_t HashLfuCache<Key, Value>::Hash(Key key)
    {
        std::hash<Key> hashFunc;
        return hashFunc(key);
    }
}
