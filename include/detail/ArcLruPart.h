#pragma once

#include <mutex>
#include <unordered_map>

#include "ArcCacheNode.h"

namespace CacheSys
{
    /**
     * @class ArcLruPart
     * @brief ARC 算法中的 LRU 分量（Recency 部分，对应 T1 和 B1）
     * 
     * 该类维护两个逻辑列表：
     * 1. Main Cache (T1): 存放实际数据的最近访问列表。
     * 2. Ghost Cache (B1): 只存放键（Key）而不存放值的列表，记录最近被驱逐出 T1 的数据信息。
     * 
     * 它支持自适应容量调整，能够根据 ARC 主算法的反馈动态增加或减少其空间配额。
     */
    template <typename Key, typename Value>
    class ArcLruPart
    {
    public:
        using NodeType = ArcNode<Key, Value>;
        using NodePtr = std::shared_ptr<NodeType>;
        using NodeMap = std::unordered_map<Key, NodePtr>;

        /**
         * @brief 构造函数
         * @param capacity T1 列表的初始物理容量
         * @param transformThreshold 频率阈值。当一个节点在 LRU 部分被访问超过此次数时，应提升至 LFU 部分。
         */
        ArcLruPart(size_t capacity, size_t transformThreshold);

        /**
         * @brief 向 LRU 部分插入新数据
         * @param key 键
         * @param value 值
         * @return bool 插入是否成功
         */
        bool put(Key key, Value value);

        /**
         * @brief 从 LRU 部分获取数据
         * @param key 要查找的键
         * @param[out] value 存储获取到的值
         * @param[out] shouldTransform 输出参数，如果为 true，表示该节点访问次数达到阈值，建议将其移至 LFU 部分
         * @return bool 是否命中
         */
        bool get(Key key, Value &value, bool &shouldTransform);

        /**
         * @brief 从缓存（含幽灵列表）中显式移除某个键
         * @return bool 移除是否成功
         */
        bool remove(Key key);

        /**
         * @brief 检查并消耗幽灵项（B1 命中）
         * 
         * 如果键存在于幽灵列表（Ghost Cache）中，将其移除并返回 true。
         * ARC 算法利用此信号来增加 T1 的目标容量（即调整参数 p）。
         * 
         * @return bool 如果在幽灵列表中找到并成功消耗，返回 true
         */
        bool consumeGhostEntry(Key key);

        /**
         * @brief 检查幽灵列表是否存在该键（别名函数）
         */
        bool checkGhost(Key key) { return consumeGhostEntry(key); }

        /**
         * @brief 增加该分量的目标容量
         * 当在 T1 幽灵列表命中时，由 ARC 主逻辑调用，提升最近访问数据的权重。
         */
        void increaseCapacity();

        /**
         * @brief 减少该分量的目标容量
         * @return bool 如果容量已经到最小值无法减少返回 false
         */
        bool decreaseCapacity();

    private:
        /**
         * @brief 初始化双向链表（创建哨兵节点）
         */
        void initializeLists();

        /**
         * @brief 内部方法：添加新节点到 T1 列表
         * 若容量不足，会触发 evictLeastRecent。
         */
        bool addNewNode(const Key &key, const Value &value);

        /**
         * @brief 内部方法：更新现有节点的访问状态
         * @param node 节点指针
         * @param value 可选的新值指针，如果不为空则更新值
         */
        bool updateNodeAccess(NodePtr node, const Value *value);

        /**
         * @brief 将节点移动到 T1 链表头部（表示最近被使用）
         */
        void moveToFront(NodePtr node);

        /**
         * @brief 将一个新节点链接到 T1 链表头部
         */
        void addToFront(NodePtr node);

        /**
         * @brief 从当前双向链表中移除节点连接
         */
        void removeFromList(NodePtr node);

        /**
         * @brief 驱逐 T1 中最久未使用的项
         * 被驱逐的项会进入幽灵列表（Ghost Cache），其值（Value）将被释放以节省内存。
         */
        void evictLeastRecent();

        /**
         * @brief 将节点（元数据）添加到幽灵列表 B1
         */
        void addToGhost(NodePtr node);

        /**
         * @brief 当幽灵列表超出上限时，移除其中最老的一项
         */
        void removeOldestGhost();

    private:
        size_t capacity_;           ///< 当前 T1 列表的容量上限（由主算法动态调整）
        size_t ghostCapacity_;      ///< 幽灵列表 B1 的容量上限
        size_t transformThreshold_; ///< 晋升为 LFU 节点的频率阈值
        std::mutex mutex_;          ///< 保护当前分量的互斥锁

        NodeMap mainCache_;  ///< T1 哈希表：存储实际数据节点
        NodeMap ghostCache_; ///< B1 哈希表：存储已被驱逐的幽灵项（Node 此时不含 Value）

        // T1 列表的哨兵节点（用于管理数据的 Recency）
        NodePtr mainHead_;  
        NodePtr mainTail_;  
        
        // B1 幽灵列表的哨兵节点
        NodePtr ghostHead_; 
        NodePtr ghostTail_; 
    };
}

#include "../../src/detail/ArcLruPart.tpp"