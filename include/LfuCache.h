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
    template <typename Key, typename Value>
    class LfuCache;

    template <typename Key, typename Value>
    class FreqList
    {
    private:
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

        void purge();

    private:
        void putInternal(Key key, Value value);
        void getInternal(NodePtr node, Value &value);
        void kickOut();
        void removeFromFreqList(NodePtr node);
        void addToFreqList(NodePtr node);
        void addFreqNum();
        void decreaseFreqNum(int num);
        void HandleOverMaxAverageNum();
        void updateMinFreq();
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

    template <typename Key, typename Value>
    class HashLfuCache
    {
    public:
        HashLfuCache(size_t capacity, int sliceNum, int maxAverageNum = 10);

        void put(Key key, Value value);
        bool get(Key key, Value &value);
        Value get(Key key);
        void purge();

    private:
        size_t Hash(Key key);

    private:
        size_t capacity_;
        int sliceNum_;
        std::vector<std::unique_ptr<LfuCache<Key, Value>>> lfuSliceCaches_;
    };
}

#include "../src/LfuCache.tpp"
