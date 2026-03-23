#pragma once

#include <list>
#include <memory>

namespace CacheSys
{
    /**
     * @brief ARC 缓存中负责管理“最近访问 (Recency)”部分的组件前置声明
     */
    template <typename Key, typename Value>
    class ArcLruPart;

    /**
     * @brief ARC 缓存中负责管理“最常访问 (Frequency)”部分的组件前置声明
     */
    template <typename Key, typename Value>
    class ArcLfuPart;

    /**
     * @class ArcNode
     * @brief ARC 缓存节点的通用结构
     * @tparam Key 键类型
     * @tparam Value 值类型
     * 
     * 该节点设计用于支持双向链表和 std::list 迭代器，
     * 能够同时在 ARC 的 LRU (Recency) 部分和 LFU (Frequency) 部分之间移动。
     */
    template <typename Key, typename Value>
    class ArcNode
    {
    public:
        /**
         * @brief 默认构造函数
         */
        ArcNode();

        /**
         * @brief 构造函数
         * @param key 键
         * @param value 值
         * 初始访问计数通常设为 1
         */
        ArcNode(Key key, Value value);

        /** @brief 获取键 */
        Key getKey() const;

        /** @brief 获取值 */
        Value getValue() const;

        /** @brief 获取访问计数 */
        size_t getAccessCount() const;

        /** @brief 更新值 */
        void setValue(const Value &value);

        /**
         * @brief 增加访问计数
         */
        void incrementAccessCount();

        /**
         * @brief 手动设置访问计数
         * @param count 目标计数值
         */
        void setAccessCount(size_t count);

    private:
        Key key_;            ///< 缓存键
        Value value_;        ///< 缓存值
        size_t accessCount_; ///< 访问频率计数

        /**
         * @brief 指向前驱节点的弱引用 (weak_ptr)
         * 使用弱引用是为了打破双向链表中的循环引用，确保内存能正确释放。
         */
        std::weak_ptr<ArcNode<Key, Value>> prev_;
        
        /**
         * @brief 指向后继节点的强引用 (shared_ptr)
         */
        std::shared_ptr<ArcNode<Key, Value>> next_;

        /**
         * @brief 频率列表迭代器
         * 存储自身在 std::list 中的位置，使得在 O(1) 时间内从列表中删除节点成为可能。
         */
        using FreqListIterator = typename std::list<std::shared_ptr<ArcNode<Key, Value>>>::iterator;
        FreqListIterator freqIter_{};
        
        /**
         * @brief 标记位：记录节点当前是否处在某个频率列表中
         */
        bool inFreqList_{false};

        // 允许 ARC 的子模块直接访问私有成员，以便高效操作链表指针和迭代器
        friend class ArcLruPart<Key, Value>;
        friend class ArcLfuPart<Key, Value>;
    };
}

// 包含模板的具体实现
#include "../../src/detail/ArcCacheNode.tpp"