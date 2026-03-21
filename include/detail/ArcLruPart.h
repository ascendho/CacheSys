#pragma once

#include <mutex>
#include <unordered_map>

#include "ArcCacheNode.h"

namespace CacheSys
{
    /**
     * @brief ARC (Adaptive Replacement Cache) 缓存的 LRU 部分实现类
     * @tparam Key 缓存键的类型（要求可哈希、可比较）
     * @tparam Value 缓存值的类型
     * @details 该类实现了 ARC 缓存中基于 LRU (最近最少使用) 策略的核心逻辑，
     * 包含主缓存区和幽灵缓存区（Ghost Cache），幽灵缓存用于记录近期被淘汰的缓存项，
     * 配合 ARC 整体策略实现自适应调整
     */
    template <typename Key, typename Value>
    class ArcLruPart
    {
    public:
        // 类型别名：ARC 缓存节点的具体类型
        using NodeType = ArcNode<Key, Value>;
        // 类型别名：指向缓存节点的共享智能指针（方便节点共享和生命周期管理）
        using NodePtr = std::shared_ptr<NodeType>;
        // 类型别名：键到节点指针的哈希映射（用于快速查找缓存节点）
        using NodeMap = std::unordered_map<Key, NodePtr>;

        /**
         * @brief 构造函数，初始化 LRU 缓存组件
         * @param capacity 主缓存区的最大容量（可存储的节点数量）
         * @param transformThreshold 触发 LRU 到 LFU 转换的阈值（访问次数阈值）
         */
        ArcLruPart(size_t capacity, size_t transformThreshold);

        /**
         * @brief 向 LRU 主缓存中添加/更新键值对
         * @param key 要添加/更新的缓存键
         * @param value 对应的缓存值
         * @return bool 操作结果：true 表示添加/更新成功，false 表示失败（如容量不足且淘汰失败）
         */
        bool put(Key key, Value value);

        /**
         * @brief 从 LRU 缓存中获取指定键的值
         * @param key 要查找的缓存键
         * @param value 输出参数，存储找到的缓存值（仅当返回 true 时有效）
         * @param shouldTransform 输出参数，标记该节点是否需要转换到 LFU 部分
         * @return bool 查找结果：true 表示命中缓存，false 表示未命中
         */
        bool get(Key key, Value &value, bool &shouldTransform);

        // 从 LRU 主缓存中删除指定 key（不存在时返回 false）。
        bool remove(Key key);

        /**
         * @brief 消费幽灵缓存中的指定条目（命中后触发缓存调整）
         * @param key 要检查的缓存键
         * @return bool 操作结果：true 表示幽灵缓存中存在该键并完成消费，false 表示不存在
         * @details 幽灵缓存命中时，会触发主缓存容量调整或节点迁移，是 ARC 自适应策略的核心步骤
         */
        bool consumeGhostEntry(Key key);

        /**
         * @brief 向后兼容的包装函数：检查幽灵缓存是否包含指定键
         * @param key 要检查的缓存键
         * @return bool 幽灵缓存是否包含该键（本质调用 consumeGhostEntry）
         * @details 为兼容旧版接口而设计，功能与 consumeGhostEntry 完全一致
         */
        bool checkGhost(Key key) { return consumeGhostEntry(key); }

        /**
         * @brief 增加 LRU 主缓存的容量
         * @details 通常在 ARC 整体策略调整时调用，比如幽灵缓存命中后扩容 LRU 部分
         */
        void increaseCapacity();

        /**
         * @brief 减少 LRU 主缓存的容量
         * @return bool 操作结果：true 表示缩容成功（淘汰了多余节点），false 表示缩容失败（无节点可淘汰）
         * @details 缩容时会淘汰最久未使用的节点，保证容量不超过新阈值
         */
        bool decreaseCapacity();

    private:
        /**
         * @brief 初始化主缓存和幽灵缓存的双向链表头/尾节点
         * @details 双向链表用于维护节点的访问顺序（头=最近访问，尾=最久未访问），
         * 初始化时会创建哨兵节点（空节点），简化链表操作（避免头/尾为空的边界判断）
         */
        void initializeLists();

        /**
         * @brief 向主缓存中添加新节点
         * @param key 缓存键
         * @param value 缓存值
         * @return bool 添加结果：true 成功，false 失败（如容量已满且无法淘汰节点）
         * @details 添加前会检查容量，若不足则先淘汰最久未使用节点，再将新节点加入链表头部
         */
        bool addNewNode(const Key &key, const Value &value);

        /**
         * @brief 更新节点的访问状态（并可选更新值）
         * @param node 要更新的节点指针
         * @param value 可选的新值（nullptr 表示仅更新访问状态，不修改值）
         * @return bool 更新结果：true 成功，false 节点无效
         * @details 访问节点时调用，会更新节点的访问次数，并判断是否达到转换阈值（shouldTransform）
         */
        bool updateNodeAccess(NodePtr node, const Value *value);

        /**
         * @brief 将节点移动到主缓存链表的头部（标记为最近访问）
         * @param node 要移动的节点指针
         * @details LRU 核心操作：最近访问的节点放在链表头，保证尾节点是最久未使用的
         */
        void moveToFront(NodePtr node);

        /**
         * @brief 将节点添加到主缓存链表的头部
         * @param node 要添加的节点指针
         * @details 新节点或更新后的节点都会添加到头部，保证访问顺序
         */
        void addToFront(NodePtr node);

        /**
         * @brief 将节点从所在的链表（主缓存/幽灵缓存）中移除
         * @param node 要移除的节点指针
         * @details 处理节点淘汰、迁移时调用，维护链表的完整性
         */
        void removeFromList(NodePtr node);

        /**
         * @brief 淘汰主缓存中最久未使用的节点（链表尾节点）
         * @details 容量不足时自动调用，淘汰的节点会被加入幽灵缓存，以便后续命中检测
         */
        void evictLeastRecent();

        /**
         * @brief 将节点添加到幽灵缓存中
         * @param node 要添加的节点指针
         * @details 淘汰的主缓存节点会进入幽灵缓存，记录近期淘汰的键，用于 ARC 策略调整
         */
        void addToGhost(NodePtr node);

        /**
         * @brief 移除幽灵缓存中最旧的节点（链表尾节点）
         * @details 当幽灵缓存容量不足时调用，保证幽灵缓存不超过设定容量
         */
        void removeOldestGhost();

    private:
        size_t capacity_;           // LRU 主缓存的最大容量（节点数量）
        size_t ghostCapacity_;      // 幽灵缓存的最大容量（通常与主缓存容量关联）
        size_t transformThreshold_; // 触发 LRU → LFU 转换的访问次数阈值
        std::mutex mutex_;          // 互斥锁，保证多线程下缓存操作的原子性（线程安全）

        NodeMap mainCache_;  // 主缓存的哈希映射：键 → 节点指针（快速查找）
        NodeMap ghostCache_; // 幽灵缓存的哈希映射：键 → 节点指针（记录淘汰的键）

        NodePtr mainHead_;  // 主缓存双向链表的头节点（最近访问的节点）
        NodePtr mainTail_;  // 主缓存双向链表的尾节点（最久未访问的节点）
        NodePtr ghostHead_; // 幽灵缓存双向链表的头节点（最近淘汰的节点）
        NodePtr ghostTail_; // 幽灵缓存双向链表的尾节点（最久淘汰的节点）
    };
}

#include "../../src/detail/ArcLruPart.tpp"