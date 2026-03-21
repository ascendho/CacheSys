#pragma once

#include <list>
#include <memory>

// 缓存系统的命名空间，用于隔离缓存相关的类和函数
namespace CacheSys
{
    // 前向声明 ARC 缓存的 LRU 部分类
    // 原因：ArcNode将其声明为友元，需要先告知编译器该类的存在
    template <typename Key, typename Value>
    class ArcLruPart;

    // 前向声明 ARC 缓存的 LFU 部分类
    // 原因：ArcNode将其声明为友元，需要先告知编译器该类的存在
    template <typename Key, typename Value>
    class ArcLfuPart;

    /**
     * @brief ARC（Adaptive Replacement Cache）缓存的节点类
     * @tparam Key 缓存键的类型（需支持比较、拷贝等基本操作）
     * @tparam Value 缓存值的类型（可自定义任意类型）
     * @details 该类封装了缓存节点的核心属性和操作，作为ARC缓存链表的基本单元
     */
    template <typename Key, typename Value>
    class ArcNode
    {
    public:
        /**
         * @brief 默认构造函数
         * @details 初始化空节点，访问计数默认置1
         */
        ArcNode();

        /**
         * @brief 带参数的构造函数
         * @details 初始化节点的键和值，访问计数初始化为1
         */
        ArcNode(Key key, Value value);

        Key getKey() const;

        Value getValue() const;

        size_t getAccessCount() const;

        void setValue(const Value &value);

        /**
         * @brief 访问次数自增（每次访问节点时调用）
         * @details 用于LFU（最少使用）策略的计数统计
         */
        void incrementAccessCount();

        /**
         * @brief 直接设置节点的访问次数
         * @param count 目标访问次数
         * @details 用于缓存策略调整时手动重置计数
         */
        void setAccessCount(size_t count);

    private:
        Key key_;            // 缓存节点的键（唯一标识节点）
        Value value_;        // 缓存节点存储的值
        size_t accessCount_; // 节点的访问计数（LFU策略核心属性）

        // 前一个节点的弱指针：避免shared_ptr循环引用导致内存泄漏
        std::weak_ptr<ArcNode<Key, Value>> prev_;
        // 后一个节点的共享指针：方便链表遍历和内存管理
        std::shared_ptr<ArcNode<Key, Value>> next_;

        // ArcLfuPart 使用：记录节点在频率链表中的位置，避免线性 remove。
        using FreqListIterator = typename std::list<std::shared_ptr<ArcNode<Key, Value>>>::iterator;
        FreqListIterator freqIter_{};
        bool inFreqList_{false};

        // 友元声明：允许ArcLruPart和ArcLfuPart访问私有成员
        // 原因：这两个类负责管理节点的链表结构，需要直接操作prev_/next_等私有成员
        friend class ArcLruPart<Key, Value>;
        friend class ArcLfuPart<Key, Value>;
    };
}

#include "../../src/detail/ArcCacheNode.tpp"