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
    // 前向声明：节点结构中的友元关系需要先声明 LruCache。
    template <typename Key, typename Value>
    class LruCache;

    // LRU 双向链表节点。
    // 每个节点保存键值对以及访问计数；prev_/next_ 用于维护最近访问顺序。
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
        // 使用 weak_ptr 打破双向链表可能产生的循环引用。
        std::weak_ptr<LruNode<Key, Value>> prev_;
        std::shared_ptr<LruNode<Key, Value>> next_;

        friend class LruCache<Key, Value>;
    };

    // 基础 LRU 缓存实现。
    // 核心结构：哈希表 + 双向链表。
    // - 哈希表负责 O(1) 查找
    // - 双向链表负责维护“最近使用顺序”
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

        // 从缓存中显式删除某个 key。
        void remove(Key key) override;

    private:
        // 初始化带哨兵节点的双向链表。
        void initializeList();

        // key 已存在时：更新 value 并将节点移动到“最近使用”位置。
        void updateExistingNode(NodePtr node, const Value &value);

        // key 不存在时：必要时先淘汰，再插入新节点。
        void addNewNode(const Key &key, const Value &value);

        // 将节点移动到链表尾部（最近使用位置）。
        void moveToMostRecent(NodePtr node);

        // 从双向链表中摘除节点。
        void removeNode(NodePtr node);

        // 插入到链表尾部。
        void insertNode(NodePtr node);

        // 淘汰最久未使用节点（链表头部有效节点）。
        void evictLeastRecent();

    private:
        int capacity_;
        NodeMap nodeMap_;
        std::mutex mutex_;
        NodePtr dummyHead_;
        NodePtr dummyTail_;
    };

    // LRU-K：在基础 LRU 之上增加“访问次数阈值”过滤。
    // 只有被访问达到 k 次的数据才进入主缓存，用于减轻一次性冷数据污染。
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

    // 分片版 LRU。
    // 通过哈希把 key 分散到多个独立 LRU 分片，以降低全局锁竞争。
    template <typename Key, typename Value>
    class ShardedLruCache
    {
    public:
        ShardedLruCache(size_t capacity, int sliceNum);

        void put(Key key, Value value);
        bool get(Key key, Value &value);
        Value get(Key key);

    private:
        // 计算 key 所属分片。
        size_t hashKey(const Key &key) const;

    private:
        size_t capacity_;
        int sliceNum_;
        std::vector<std::unique_ptr<LruCache<Key, Value>>> lruSliceCaches_;
    };

    // 向后兼容旧类名，避免现有调用代码立即失效。
    template <typename Key, typename Value>
    using HashLruCaches = ShardedLruCache<Key, Value>;
}

#include "../src/LruCache.tpp"
