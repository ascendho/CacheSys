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
    // 前置声明
    template <typename Key, typename Value>
    class LfuCache;

    /**
     * @class FreqList
     * @brief 频率链表：存储具有相同访问频率的所有节点
     * 
     * LFU 算法的核心组成部分。每个 FreqList 对应一个特定的访问次数（频率）。
     * 内部使用双向链表结构。
     */
    template <typename Key, typename Value>
    class FreqList
    {
    private:
        /**
         * @struct Node
         * @brief 实际存储数据的缓存节点
         */
        struct Node
        {
            int freq;                   ///< 该节点被访问的频率
            Key key;                    ///< 缓存键
            Value value;                ///< 缓存值
            std::weak_ptr<Node> pre;    ///< 指向前驱节点（弱引用，防止循环引用）
            std::shared_ptr<Node> next; ///< 指向后继节点（强引用）

            Node();
            Node(Key key, Value value);
        };

        using NodePtr = std::shared_ptr<Node>;

    public:
        /**
         * @brief 构造函数
         * @param n 该链表代表的频率值
         */
        explicit FreqList(int n); 

        bool isEmpty() const;          ///< 检查当前频率下是否有节点
        void addNode(NodePtr node);    ///< 将节点插入到该频率链表的头部（最新访问）
        void removeNode(NodePtr node); ///< 从该频率链表中移除指定节点
        NodePtr getFirstNode() const;  ///< 获取该频率下最旧的节点（通常用于淘汰）

    private:
        int freq_;     ///< 关联的频率值
        NodePtr head_; ///< 哨兵头节点
        NodePtr tail_; ///< 哨兵尾节点

        friend class LfuCache<Key, Value>;
    };

    /**
     * @class LfuCache
     * @brief LFU（最近最少使用）缓存策略实现
     * @extends CachePolicy
     * 
     * 算法原理：当缓存满时，优先淘汰访问次数最少的数据。
     * 时间复杂度：put/get 均为 O(1)。
     */
    template <typename Key, typename Value>
    class LfuCache : public CachePolicy<Key, Value>
    {
    public:
        using Node = typename FreqList<Key, Value>::Node;
        using NodePtr = std::shared_ptr<Node>;
        using NodeMap = std::unordered_map<Key, NodePtr>;

        /**
         * @brief 构造函数
         * @param capacity 缓存容量
         * @param maxAverageNum 用于频率溢出处理或衰减机制的阈值
         */
        explicit LfuCache(int capacity, int maxAverageNum = 1000000);
        ~LfuCache() override;

        void put(Key key, Value value) override;  
        bool get(Key key, Value &value) override; 
        Value get(Key key) override;              

        /**
         * @brief 强制清空缓存
         */
        void purge(); 

    private:
        void putInternal(Key key, Value value);       ///< 内部插入逻辑
        void getInternal(NodePtr node, Value &value); ///< 内部读取及频率更新逻辑
        void evictOneEntry();                         ///< 淘汰一个频率最低且最旧的项

        void removeFromFreqList(NodePtr node); ///< 将节点从当前的频率桶中移除
        void addToFreqList(NodePtr node);      ///< 将节点加入到新的频率桶（freq+1）

        // 频率管理相关（防止频率计数过大导致性能问题或溢出）
        void addFreqNum();              
        void decreaseFreqNum(int num);  
        void handleFrequencyOverflow(); ///< 处理频率溢出或进行频率衰减

        void updateMinFreq();           ///< 更新全局最小频率计数器，用于快速定位淘汰目标
        void clearFreqLists();          ///< 释放所有频率桶

    private:
        int capacity_;                                                   ///< 缓存最大容量
        int minFreq_;                                                    ///< 当前所有数据中的最小访问频率
        int maxAverageNum_;                                              ///< 平均频率阈值（用于触发机制）
        int curAverageNum_;                                              ///< 当前平均频率记录
        int curTotalNum_;                                                ///< 当前节点总数
        std::mutex mutex_;                                               ///< 线程安全锁
        NodeMap nodeMap_;                                                ///< Key 到 Node 的映射（O(1) 查找）
        
        /**
         * @brief 频率桶映射表
         * Key: 访问频率, Value: 该频率对应的所有节点链表
         */
        std::unordered_map<int, std::unique_ptr<FreqList<Key, Value>>> freqToFreqList_; 
    };

    /**
     * @class ShardedLfuCache
     * @brief 分片 LFU 缓存，用于降低高并发下的锁竞争
     */
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
        ///< 计算键所属的分片索引
        size_t hashKey(const Key &key) const; 

    private:
        size_t capacity_;                                                   ///< 总容量
        int sliceNum_;                                                      ///< 分片数量
        std::vector<std::unique_ptr<LfuCache<Key, Value>>> lfuSliceCaches_; ///< 各个分片的实例
    };

    /**
     * @brief 别名：哈希分片 LFU 缓存
     */
    template <typename Key, typename Value>
    using HashLfuCache = ShardedLfuCache<Key, Value>;
}

// 包含具体实现
#include "../src/LfuCache.tpp"