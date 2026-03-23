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
    // 前置声明
    template <typename Key, typename Value>
    class LruCache;

    /**
     * @class LruNode
     * @brief LRU 缓存的双向链表节点类
     * @tparam Key 键类型
     * @tparam Value 值类型
     * 
     * 存储数据、访问统计以及指向前后节点的指针。
     * 使用 weak_ptr 指向前驱节点以防止循环引用导致的内存泄漏。
     */
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
        Key key_;                       ///< 缓存键
        Value value_;                   ///< 缓存值
        size_t accessCount_;            ///< 累计访问次数（用于 LRU-K 等高级策略）
        
        // 双向链表指针
        std::weak_ptr<LruNode<Key, Value>> prev_;   ///< 指向前驱节点（弱引用）
        std::shared_ptr<LruNode<Key, Value>> next_; ///< 指向后继节点（强引用）

        // 允许 LruCache 访问私有成员以便进行链表操作
        friend class LruCache<Key, Value>;
    };

    /**
     * @class LruCache
     * @brief 标准最近最少使用（LRU）缓存实现
     * @extends CachePolicy
     * 
     * 使用 哈希表 (std::unordered_map) 实现 O(1) 查找，
     * 使用 双向链表 实现 O(1) 的数据位置更新。
     */
    template <typename Key, typename Value>
    class LruCache : public CachePolicy<Key, Value>
    {
    public:
        using LruNodeType = LruNode<Key, Value>;          
        using NodePtr = std::shared_ptr<LruNodeType>;     
        using NodeMap = std::unordered_map<Key, NodePtr>; 

        /**
         * @brief 构造函数
         * @param capacity 缓存最大容量
         */
        explicit LruCache(int capacity);

        /**
         * @brief 析构函数
         */
        ~LruCache() override = default;

        /**
         * @brief 插入或更新缓存项
         * 如果键已存在，则更新值并移至链表头部；
         * 如果不存在，插入新节点；若超过容量，则淘汰最久未使用的节点。
         */
        void put(Key key, Value value) override;

        /**
         * @brief 获取缓存项（引用方式）
         * @return bool 是否命中。命中则通过 value 参数返回，并提升该节点优先级。
         */
        bool get(Key key, Value &value) override;

        /**
         * @brief 获取缓存项（直接返回）
         * @return Value 命中的值，未命中返回默认值。
         */
        Value get(Key key) override;

        /**
         * @brief 显式移除某个键对应的缓存
         */
        void remove(Key key) override;

    private:
        /**
         * @brief 初始化双向链表，创建哨兵节点（dummyHead/dummyTail）
         */
        void initializeList();

        /**
         * @brief 更新已有节点的值并提升优先级
         */
        void updateExistingNode(NodePtr node, const Value &value);

        /**
         * @brief 插入全新的节点到链表头部
         */
        void addNewNode(const Key &key, const Value &value);

        /**
         * @brief 将节点移动到链表头部（表示最近被访问）
         */
        void moveToMostRecent(NodePtr node);

        /**
         * @brief 从双向链表中断开该节点
         */
        void removeNode(NodePtr node);

        /**
         * @brief 在 dummyHead 之后插入节点
         */
        void insertNode(NodePtr node);

        /**
         * @brief 淘汰链表尾部（最久未使用）的节点
         */
        void evictLeastRecent();

    private:
        int capacity_;           ///< 缓存最大容量限制
        NodeMap nodeMap_;        ///< 哈希表：用于快速定位节点
        std::mutex mutex_;       ///< 互斥锁：保证单机缓存的操作原子性
        NodePtr dummyHead_;      ///< 哨兵头节点（最新访问）
        NodePtr dummyTail_;      ///< 哨兵尾节点（最旧访问）
    };

    /**
     * @class LruKCache
     * @brief LRU-K 缓存算法实现
     * @extends LruCache
     * 
     * 解决“缓存污染”问题。只有当某个数据被访问过 K 次后，才将其放入正式缓存中。
     * 内部维护一个“历史队列”来记录访问次数。
     */
    template <typename Key, typename Value>
    class LruKCache : public LruCache<Key, Value>
    {
    public:
        /**
         * @brief 构造函数
         * @param capacity 正式缓存容量
         * @param historyCapacity 历史队列容量
         * @param k 触发淘汰/晋升的阈值次数
         */
        LruKCache(int capacity, int historyCapacity, int k);

        /**
         * @brief 获取数据，并更新访问次数
         */
        Value get(Key key);

        /**
         * @brief 放入数据。若访问不满 K 次，仅在历史队列中记录；达到 K 次后进入正式缓存。
         */
        void put(Key key, Value value);

    private:
        int k_;                                              ///< 进入正式缓存所需的最小访问次数
        std::unique_ptr<LruCache<Key, size_t>> historyList_; ///< 历史队列：记录访问次数
        std::unordered_map<Key, Value> historyValueMap_;     ///< 历史数据存储
        std::mutex lruKMutex_;                               ///< LRU-K 专用锁
    };

    /**
     * @class ShardedLruCache
     * @brief 分片（分段）LRU 缓存
     * 
     * 通过将缓存划分为多个独立的分片 (Shards)，减少在高并发场景下的锁竞争。
     * 每个分片拥有独立的锁和 LRU 实例。
     */
    template <typename Key, typename Value>
    class ShardedLruCache
    {
    public:
        /**
         * @brief 构造函数
         * @param capacity 总容量
         * @param sliceNum 分片数量（通常设为 CPU 核心数的倍数）
         */
        ShardedLruCache(size_t capacity, int sliceNum);

        void put(Key key, Value value);
        bool get(Key key, Value &value);
        Value get(Key key);

    private:
        /**
         * @brief 哈希函数，用于决定键属于哪一个分片
         */
        size_t hashKey(const Key &key) const;

    private:
        size_t capacity_; ///< 总容量
        int sliceNum_;    ///< 分片总数

        /// 存储各个分片的 LRU 实例
        std::vector<std::unique_ptr<LruCache<Key, Value>>> lruSliceCaches_;
    };

    /**
     * @brief 别名：HashLruCaches 等同于 ShardedLruCache
     */
    template <typename Key, typename Value>
    using HashLruCaches = ShardedLruCache<Key, Value>;
}

// 包含模板类的具体实现
#include "../src/LruCache.tpp"