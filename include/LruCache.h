#pragma once

#include <cmath>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "CachePolicy.h"

namespace CacheSys
{
    template <typename Key, typename Value>
    class LruCache;

    template <typename Key, typename Value>
    class LruNode
    {
    public:
        LruNode(Key key, Value value);

        Key getKey() const;
        Value getValue() const;
        void setValue(const Value &value);
        size_t getAccessCount() const;
        void incrementAccessCount();

    private:
        Key key_;
        Value value_;
        size_t accessCount_;
        std::weak_ptr<LruNode<Key, Value>> prev_;
        std::shared_ptr<LruNode<Key, Value>> next_;

        friend class LruCache<Key, Value>;
    };

    template <typename Key, typename Value>
    class LruCache : public CachePolicy<Key, Value>
    {
    public:
        using LruNodeType = LruNode<Key, Value>;
        using NodePtr = std::shared_ptr<LruNodeType>;
        using NodeMap = std::unordered_map<Key, NodePtr>;

        explicit LruCache(int capacity);
        ~LruCache() override = default;

        void put(Key key, Value value) override;
        bool get(Key key, Value &value) override;
        Value get(Key key) override;

        void remove(Key key);

    private:
        void initializeList();
        void updateExistingNode(NodePtr node, const Value &value);
        void addNewNode(const Key &key, const Value &value);
        void moveToMostRecent(NodePtr node);
        void removeNode(NodePtr node);
        void insertNode(NodePtr node);
        void evictLeastRecent();

    private:
        int capacity_;
        NodeMap nodeMap_;
        std::mutex mutex_;
        NodePtr dummyHead_;
        NodePtr dummyTail_;
    };

    template <typename Key, typename Value>
    class LruKCache : public LruCache<Key, Value>
    {
    public:
        LruKCache(int capacity, int historyCapacity, int k);

        Value get(Key key);
        void put(Key key, Value value);

    private:
        int k_;
        std::unique_ptr<LruCache<Key, size_t>> historyList_;
        std::unordered_map<Key, Value> historyValueMap_;
    };

    template <typename Key, typename Value>
    class HashLruCaches
    {
    public:
        HashLruCaches(size_t capacity, int sliceNum);

        void put(Key key, Value value);
        bool get(Key key, Value &value);
        Value get(Key key);

    private:
        size_t Hash(Key key);

    private:
        size_t capacity_;
        int sliceNum_;
        std::vector<std::unique_ptr<LruCache<Key, Value>>> lruSliceCaches_;
    };
}

#include "../src/LruCache.tpp"
