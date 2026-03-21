#pragma once

#include <list>
#include <map>
#include <mutex>
#include <unordered_map>

#include "ArcCacheNode.h"

namespace CacheSys
{
    /**
     * @brief ARC (Adaptive Replacement Cache) 缓存的 LFU 部分实现类
     * @tparam Key 缓存键的类型（要求可哈希、可比较）
     * @tparam Value 缓存值的类型
     * @details 该类实现了 ARC 缓存中基于 LFU (最不经常使用) 策略的核心逻辑，
     * 核心是按「访问频率」管理缓存节点：频率越低的节点越容易被淘汰；
     * 同时包含幽灵缓存（Ghost Cache）记录近期淘汰的节点，配合 ARC 整体策略实现自适应调整。
     */
    template <typename Key, typename Value>
    class ArcLfuPart
    {
    public:
        // 类型别名：ARC 缓存节点的具体类型（简化代码书写）
        using NodeType = ArcNode<Key, Value>;
        // 类型别名：指向缓存节点的共享智能指针（方便节点生命周期管理和跨组件共享）
        using NodePtr = std::shared_ptr<NodeType>;
        // 类型别名：键到节点指针的哈希映射（用于快速查找主缓存/幽灵缓存中的节点）
        using NodeMap = std::unordered_map<Key, NodePtr>;
        // 类型别名：访问频率到节点列表的有序映射（LFU 核心）
        // key=访问频率，value=该频率下的所有节点（双向链表，保证插入/删除效率）
        using FreqMap = std::map<size_t, std::list<NodePtr>>;

        /**
         * @brief 构造函数，初始化 LFU 缓存组件
         * @param capacity 主缓存区的最大容量（可存储的节点数量）
         * @param transformThreshold 触发 LFU 到 LRU 转换的阈值（频率阈值）
         */
        ArcLfuPart(size_t capacity, size_t transformThreshold);

        /**
         * @brief 向 LFU 主缓存中添加/更新键值对
         * @param key 要添加/更新的缓存键
         * @param value 对应的缓存值
         * @return bool 操作结果：true 表示添加/更新成功，false 表示失败（如容量不足且淘汰失败）
         */
        bool put(Key key, Value value);

        /**
         * @brief 从 LFU 缓存中获取指定键的值
         * @param key 要查找的缓存键
         * @param value 输出参数，存储找到的缓存值（仅当返回 true 时有效）
         * @return bool 查找结果：true 表示命中缓存，false 表示未命中
         * @details 命中时会更新节点的访问频率（LFU 核心逻辑），影响后续淘汰策略
         */
        bool get(Key key, Value &value);

        /**
         * @brief 检查主缓存中是否包含指定键
         * @param key 要检查的缓存键
         * @return bool 存在性结果：true 表示包含，false 表示不包含
         */
        bool contains(Key key);

        /**
         * @brief 消费幽灵缓存中的指定条目（命中后触发缓存策略调整）
         * @param key 要检查的缓存键
         * @return bool 操作结果：true 表示幽灵缓存中存在该键并完成消费，false 表示不存在
         * @details 幽灵缓存命中时，会触发主缓存容量调整或节点迁移，是 ARC 自适应策略的关键步骤
         */
        bool consumeGhostEntry(Key key);

        /**
         * @brief 向后兼容的包装函数：检查主缓存是否包含指定键（旧接口）
         * @param key 要检查的缓存键
         * @return bool 主缓存是否包含该键（本质调用 contains 方法）
         */
        bool contain(Key key) { return contains(key); }

        /**
         * @brief 向后兼容的包装函数：检查幽灵缓存是否包含指定键（旧接口）
         * @param key 要检查的缓存键
         * @return bool 幽灵缓存是否包含该键（本质调用 consumeGhostEntry 方法）
         */
        bool checkGhost(Key key) { return consumeGhostEntry(key); }

        /**
         * @brief 增加 LFU 主缓存的容量
         * @details 通常在 ARC 整体策略调整时调用（如幽灵缓存命中后扩容 LFU 部分）
         */
        void increaseCapacity();

        /**
         * @brief 减少 LFU 主缓存的容量
         * @return bool 操作结果：true 表示缩容成功（淘汰了多余节点），false 表示缩容失败（无节点可淘汰）
         * @details 缩容时会淘汰最不经常使用的节点，保证容量不超过新阈值
         */
        bool decreaseCapacity();

    private:
        /**
         * @brief 初始化幽灵缓存的双向链表头/尾节点
         * @details 创建哨兵节点（空节点），简化链表的插入/删除操作（避免头/尾为空的边界判断）
         */
        void initializeLists();

        /**
         * @brief 向主缓存中添加新节点
         * @param key 缓存键
         * @param value 缓存值
         * @return bool 添加结果：true 成功，false 失败（如容量已满且无法淘汰节点）
         * @details 添加前会检查容量，若不足则先调用 evictLeastFrequent 淘汰最不经常使用的节点
         */
        bool addNewNode(const Key &key, const Value &value);

        /**
         * @brief 更新主缓存中已存在的节点值
         * @param node 要更新的节点指针
         * @param value 新的缓存值
         * @return bool 更新结果：true 成功，false 节点无效
         * @details 仅更新值，访问频率的更新由 updateNodeFrequency 单独处理
         */
        bool updateExistingNode(NodePtr node, const Value &value);

        /**
         * @brief 更新节点的访问频率（LFU 核心逻辑）
         * @param node 要更新的节点指针
         * @details 1. 将节点从原频率的列表中移除；
         *          2. 将节点加入「原频率+1」的列表中；
         *          3. 维护 minFreq_（当前最小访问频率），保证淘汰时能快速定位最不常用节点
         */
        void updateNodeFrequency(NodePtr node);

        /**
         * @brief 淘汰主缓存中最不经常使用的节点（LFU 核心操作）
         * @details 1. 找到 minFreq_ 对应的节点列表，移除链表首节点（最久未访问的低频节点）；
         *          2. 将淘汰的节点加入幽灵缓存；
         *          3. 更新 minFreq_（若当前频率列表为空，递增 minFreq_）
         */
        void evictLeastFrequent();

        /**
         * @brief 将节点从其所属的频率列表中移除
         * @param node 要移除的节点指针
         * @details 配合 updateNodeFrequency 使用，维护 freqMap_ 的完整性
         */
        void removeFromList(NodePtr node);

        /**
         * @brief 将节点添加到幽灵缓存中
         * @param node 要添加的节点指针
         * @details 淘汰的主缓存节点会进入幽灵缓存，记录近期淘汰的键，用于 ARC 策略自适应调整
         */
        void addToGhost(NodePtr node);

        /**
         * @brief 移除幽灵缓存中最旧的节点（链表尾节点）
         * @details 当幽灵缓存容量不足时调用，保证幽灵缓存不超过 ghostCapacity_
         */
        void removeOldestGhost();

    private:
        size_t capacity_;           // LFU 主缓存的最大容量（节点数量）
        size_t ghostCapacity_;      // 幽灵缓存的最大容量（通常与主缓存容量关联）
        size_t transformThreshold_; // 触发 LFU → LRU 转换的频率阈值
        size_t minFreq_;            // 当前主缓存中最小的访问频率（LFU 核心，快速定位最不常用节点）
        std::mutex mutex_;          // 互斥锁，保证多线程下所有缓存操作的原子性（线程安全）

        NodeMap mainCache_;  // 主缓存的哈希映射：键 → 节点指针（O(1) 快速查找）
        NodeMap ghostCache_; // 幽灵缓存的哈希映射：键 → 节点指针（记录淘汰的键）
        FreqMap freqMap_;    // 频率映射：访问频率 → 节点列表（LFU 核心，按频率分组管理节点）

        NodePtr ghostHead_; // 幽灵缓存双向链表的头节点（最近淘汰的节点）
        NodePtr ghostTail_; // 幽灵缓存双向链表的尾节点（最久淘汰的节点）
    };
}

#include "../../src/detail/ArcLfuPart.tpp"