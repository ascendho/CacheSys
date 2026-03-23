#pragma once

#include <list>
#include <map>
#include <mutex>
#include <unordered_map>

#include "ArcCacheNode.h"

namespace CacheSys
{
    /**
     * @class ArcLfuPart
     * @brief ARC 算法中的 LFU 分量（Frequency 部分，对应 T2 和 B2）
     * 
     * 该类负责管理至少被访问过两次（或达到晋升阈值）的数据：
     * 1. Main Cache (T2): 存放实际数据，按访问频率排序（相同频率按 LRU 排序）。
     * 2. Ghost Cache (B2): 存放从 T2 驱逐出来的键，用于捕获“长期热点”的再次回归。
     * 
     * 设计特点：
     * - 使用 FreqMap（频率桶）实现 O(1) 或 O(log F) 的频率更新。
     * - 支持动态容量调整，与 LRU 部分共同平衡缓存策略。
     */
    template <typename Key, typename Value>
    class ArcLfuPart
    {
    public:
        using NodeType = ArcNode<Key, Value>;
        using NodePtr = std::shared_ptr<NodeType>;
        using NodeMap = std::unordered_map<Key, NodePtr>;
        
        /**
         * @brief 频率桶映射
         * Key: 访问次数，Value: 该次数下所有节点的双向链表（按访问时间排序）
         */
        using FreqMap = std::map<size_t, std::list<NodePtr>>;

        /**
         * @brief 构造函数
         * @param capacity T2 列表的初始物理容量
         * @param transformThreshold 频率阈值（虽然主要由 LRU 部分使用，但此处保持一致性）
         */
        ArcLfuPart(size_t capacity, size_t transformThreshold);

        /**
         * @brief 向 LFU 部分插入或更新数据
         * 如果 key 已存在，则更新值并提升其频率。
         * @return bool 是否执行了插入/更新
         */
        bool put(Key key, Value value);

        /**
         * @brief 从 LFU 部分获取数据
         * 命中后会自动调用 updateNodeFrequency 提升其频率桶位置。
         * @return bool 是否命中
         */
        bool get(Key key, Value &value);

        /**
         * @brief 检查某个键是否存在于活跃的 T2 缓存中
         */
        bool contains(Key key);

        /**
         * @brief 检查并消耗幽灵项（B2 命中）
         * 
         * 如果在 B2 幽灵列表中找到该键，将其移除并返回 true。
         * ARC 主算法收到此信号后，通常会增加 LFU 部分的权重（减小 p 值）。
         * @return bool 如果在幽灵列表中找到并成功移除，返回 true
         */
        bool consumeGhostEntry(Key key);

        /** @brief 别名：检查 T2 是否包含该键 */
        bool contain(Key key) { return contains(key); }

        /** @brief 别名：检查并消耗 B2 幽灵项 */
        bool checkGhost(Key key) { return consumeGhostEntry(key); }

        /**
         * @brief 增加该分量的目标容量（自适应调整）
         */
        void increaseCapacity();

        /**
         * @brief 减少该分量的目标容量
         * @return bool 如果容量已达最小无法减少返回 false
         */
        bool decreaseCapacity();

    private:
        /**
         * @brief 初始化幽灵链表的哨兵节点
         */
        void initializeLists();

        /**
         * @brief 内部方法：将新节点直接加入 T2 列表
         * 通常发生在节点从 LRU 部分晋升（Transform）过来时。
         */
        bool addNewNode(const Key &key, const Value &value);

        /**
         * @brief 内部方法：更新 T2 中已存在节点的值
         */
        bool updateExistingNode(NodePtr node, const Value &value);

        /**
         * @brief 频率提升逻辑
         * 将节点从当前的频率桶移出，放入 freq + 1 的桶中。
         */
        void updateNodeFrequency(NodePtr node);

        /**
         * @brief 淘汰 T2 中访问频率最低且最旧的数据
         * 被淘汰的节点会失去 Value，其 Key 进入 B2 幽灵列表。
         */
        void evictLeastFrequent();

        /**
         * @brief 从当前的频率桶（FreqMap）中彻底移除节点
         */
        void removeFromList(NodePtr node);

        /**
         * @brief 将节点元数据加入 B2 幽灵列表
         */
        void addToGhost(NodePtr node);

        /**
         * @brief 当 B2 幽灵列表超出上限时，移除最老的幽灵项
         */
        void removeOldestGhost();

    private:
        size_t capacity_;           ///< 当前 T2 列表的最大容量配额
        size_t ghostCapacity_;      ///< B2 幽灵列表的容量上限
        size_t transformThreshold_; ///< 晋升阈值
        size_t minFreq_;            ///< 当前缓存中的最小频率（优化查找）
        std::mutex mutex_;          ///< 分量互斥锁

        NodeMap mainCache_;  ///< T2 哈希表：Key -> NodePtr (存储实际数据)
        NodeMap ghostCache_; ///< B2 哈希表：Key -> NodePtr (仅存储元数据)
        FreqMap freqMap_;    ///< 频率桶：实现按频率淘汰的核心结构

        // B2 幽灵列表的哨兵节点（幽灵列表通常使用简单的 LRU 顺序淘汰）
        NodePtr ghostHead_; 
        NodePtr ghostTail_; 
    };
}

#include "../../src/detail/ArcLfuPart.tpp"