#pragma once

#include <cmath>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "CachePolicy.h"

namespace CacheSys
{
    // 前向声明：FreqList 中的友元关系需要提前声明 LfuCache。
    template <typename Key, typename Value>
    class LfuCache;

    // 频率桶（Frequency List）。
    // 每个 FreqList 实例管理“同一访问频率”的一组节点。
    template <typename Key, typename Value>
    class FreqList
    {
    private:
        // LFU 内部节点结构：保存 key/value/freq 以及双向链表指针。
        struct Node
        {
            int freq;
            Key key;
            Value value;
            std::weak_ptr<Node> pre;
            std::shared_ptr<Node> next;

            Node();
            Node(Key key, Value value);
        };

        using NodePtr = std::shared_ptr<Node>;

    public:
        // n 表示该频率桶对应的频率值。
        explicit FreqList(int n);

        bool isEmpty() const;
        void addNode(NodePtr node);
        void removeNode(NodePtr node);
        NodePtr getFirstNode() const;

    private:
        int freq_;
        NodePtr head_;
        NodePtr tail_;

        friend class LfuCache<Key, Value>;
    };

    // 基础 LFU 缓存实现。
    // 核心结构：
    // - nodeMap_：key -> node，负责 O(1) 查找
    // - freqToFreqList_：freq -> bucket，负责按频率组织节点
    // - minFreq_：当前最小频率，淘汰时可 O(1) 定位候选桶
    template <typename Key, typename Value>
    class LfuCache : public CachePolicy<Key, Value>
    {
    public:
        using Node = typename FreqList<Key, Value>::Node;
        using NodePtr = std::shared_ptr<Node>;
        using NodeMap = std::unordered_map<Key, NodePtr>;

        explicit LfuCache(int capacity, int maxAverageNum = 1000000);
        ~LfuCache() override;

        void put(Key key, Value value) override;
        bool get(Key key, Value &value) override;
        Value get(Key key) override;

        // 清空缓存内容与频率桶。
        void purge();

    private:
        // 插入新节点的内部流程。
        void putInternal(Key key, Value value);

        // 命中后更新频率并回填 value 的内部流程。
        void getInternal(NodePtr node, Value &value);

        // 淘汰一个节点（通常是最小频率桶中的最早节点）。
        void evictOneEntry();

        // 节点在桶之间迁移时的辅助方法。
        void removeFromFreqList(NodePtr node);
        void addToFreqList(NodePtr node);

        // 维护频率统计与衰减。
        void addFreqNum();
        void decreaseFreqNum(int num);
        void handleFrequencyOverflow();

        // 维护当前最小频率。
        void updateMinFreq();

        // 释放所有动态分配的频率桶。
        void clearFreqLists();

    private:
        int capacity_;
        int minFreq_;
        int maxAverageNum_;
        int curAverageNum_;
        int curTotalNum_;
        std::mutex mutex_;
        NodeMap nodeMap_;
        std::unordered_map<int, FreqList<Key, Value> *> freqToFreqList_;
    };

    // 分片版 LFU。
    // 将请求按哈希分散到多个独立 LFU 分片，降低并发锁竞争。
    template <typename Key, typename Value>
    class ShardedLfuCache
    {
    public:
        ShardedLfuCache(size_t capacity, int sliceNum, int maxAverageNum = 10);

        void put(Key key, Value value);
        bool get(Key key, Value &value);
        Value get(Key key);
        void purge();

    private:
        // 根据 key 计算分片索引。
        size_t hashKey(const Key &key) const;

    private:
        size_t capacity_;
        int sliceNum_;
        std::vector<std::unique_ptr<LfuCache<Key, Value>>> lfuSliceCaches_;
    };

    // 向后兼容旧类名，避免现有调用代码立即失效。
    template <typename Key, typename Value>
    using HashLfuCache = ShardedLfuCache<Key, Value>;
}

#include "../src/LfuCache.tpp"
