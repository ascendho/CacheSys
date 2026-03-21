#pragma once

#include "../include/LfuCache.h"

namespace CacheSys
{
    // ===== FreqList::Node 实现 =====

    // 默认构造：频率初始化为1，后继指针置空
    template <typename Key, typename Value>
    FreqList<Key, Value>::Node::Node() : freq(1), next(nullptr) {}

    // 带参构造：初始化键值，频率默认1
    template <typename Key, typename Value>
    FreqList<Key, Value>::Node::Node(Key key, Value value) : freq(1), key(key), value(value), next(nullptr) {}

    // ===== FreqList 实现 =====

    // 构造频率桶：绑定对应频率值，初始化哨兵链表
    template <typename Key, typename Value>
    FreqList<Key, Value>::FreqList(int n) : freq_(n)
    {
        head_ = std::make_shared<Node>();
        tail_ = std::make_shared<Node>();
        head_->next = tail_;
        tail_->pre = head_;
    }

    // 判断频率桶是否为空（无有效节点）
    template <typename Key, typename Value>
    bool FreqList<Key, Value>::isEmpty() const
    {
        return head_->next == tail_;
    }

    // 添加节点到频率桶尾部（同频率最新访问）
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

    // 从频率桶中移除指定节点
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

    // 获取频率桶内首个有效节点（同频率最久未访问）
    template <typename Key, typename Value>
    typename FreqList<Key, Value>::NodePtr FreqList<Key, Value>::getFirstNode() const
    {
        return head_->next;
    }

    // ===== LfuCache 实现 =====

    // 构造LFU缓存：初始化容量、频率阈值及统计变量
    template <typename Key, typename Value>
    LfuCache<Key, Value>::LfuCache(int capacity, int maxAverageNum)
        : capacity_(capacity),
          minFreq_(std::numeric_limits<int>::max()),
          maxAverageNum_(maxAverageNum),
          curAverageNum_(0),
          curTotalNum_(0)
    {
    }

    // 析构：释放所有频率桶内存
    template <typename Key, typename Value>
    LfuCache<Key, Value>::~LfuCache()
    {
        clearFreqLists();
    }

    // 写入缓存：存在则更新值+提升频率，不存在则新增
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

    // 读取缓存：命中更新频率，未命中更新统计
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

        getInternal(it->second, value);
        ++this->hits_;
        return true;
    }

    // 便捷读取：封装带输出参数的get方法
    template <typename Key, typename Value>
    Value LfuCache<Key, Value>::get(Key key)
    {
        Value value{};
        this->get(key, value);
        return value;
    }

    // 清空缓存：清理节点、频率桶，重置统计变量
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

    // 新增节点内部逻辑：满容先淘汰，再创建节点+加入频率桶
    template <typename Key, typename Value>
    void LfuCache<Key, Value>::putInternal(Key key, Value value)
    {
        if (nodeMap_.size() >= static_cast<size_t>(capacity_))
        {
            evictOneEntry();
        }

        NodePtr node = std::make_shared<Node>(key, value);
        nodeMap_[key] = node;
        addToFreqList(node);
        addFreqNum();
        minFreq_ = std::min(minFreq_, 1);
    }

    // 命中后更新频率：旧桶移除→频率+1→新桶插入
    template <typename Key, typename Value>
    void LfuCache<Key, Value>::getInternal(NodePtr node, Value &value)
    {
        const int oldFreq = node->freq;
        value = node->value;
        removeFromFreqList(node);
        node->freq++;
        addToFreqList(node);

        auto it = freqToFreqList_.find(oldFreq);
        if (oldFreq == minFreq_ && (it == freqToFreqList_.end() || it->second->isEmpty()))
        {
            updateMinFreq();
        }

        addFreqNum();
    }

    // 淘汰节点：从最小频率桶移除首个有效节点
    template <typename Key, typename Value>
    void LfuCache<Key, Value>::evictOneEntry()
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

    // 从对应频率桶移除节点
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
        if (freqIt->second->isEmpty())
        {
            freqToFreqList_.erase(freqIt);
        }
    }

    // 将节点加入对应频率桶（不存在则创建桶）
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
            freqToFreqList_[freq] = std::make_unique<FreqList<Key, Value>>(freq);
        }

        freqToFreqList_[freq]->addNode(node);
    }

    // 更新全局频率统计：累计访问次数+计算平均频率
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
            handleFrequencyOverflow();
        }
    }

    // 淘汰节点后更新频率统计
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

    // 频率溢出处理：所有节点频率衰减，防止数值无限增长
    template <typename Key, typename Value>
    void LfuCache<Key, Value>::handleFrequencyOverflow()
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

    // 更新全局最小频率：扫描所有非空桶找最小值
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

    // 释放所有频率桶内存，避免内存泄漏
    template <typename Key, typename Value>
    void LfuCache<Key, Value>::clearFreqLists()
    {
        freqToFreqList_.clear();
    }

    // ===== ShardedLfuCache 实现 =====

    // 构造分片LFU：初始化分片数，均分总容量到各分片
    template <typename Key, typename Value>
    ShardedLfuCache<Key, Value>::ShardedLfuCache(size_t capacity, int sliceNum, int maxAverageNum)
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

    // 写入缓存：哈希路由到对应分片
    template <typename Key, typename Value>
    void ShardedLfuCache<Key, Value>::put(Key key, Value value)
    {
        size_t sliceIndex = hashKey(key) % static_cast<size_t>(sliceNum_);
        lfuSliceCaches_[sliceIndex]->put(key, value);
    }

    // 读取缓存：哈希路由到对应分片
    template <typename Key, typename Value>
    bool ShardedLfuCache<Key, Value>::get(Key key, Value &value)
    {
        size_t sliceIndex = hashKey(key) % static_cast<size_t>(sliceNum_);
        return lfuSliceCaches_[sliceIndex]->get(key, value);
    }

    // 便捷读取：封装带输出参数的get方法
    template <typename Key, typename Value>
    Value ShardedLfuCache<Key, Value>::get(Key key)
    {
        Value value{};
        get(key, value);
        return value;
    }

    // 清空所有分片缓存
    template <typename Key, typename Value>
    void ShardedLfuCache<Key, Value>::purge()
    {
        for (auto &lfuSliceCache : lfuSliceCaches_)
        {
            lfuSliceCache->purge();
        }
    }

    // 哈希计算：获取key对应的分片索引
    template <typename Key, typename Value>
    size_t ShardedLfuCache<Key, Value>::hashKey(const Key &key) const
    {
        std::hash<Key> hashFunc;
        return hashFunc(key);
    }
}