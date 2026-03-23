#pragma once

namespace CacheSys
{
    /**
     * @brief 默认构造函数
     * 
     * 初始化一个空节点。
     * - accessCount_ 设为 1：表示节点被创建即代表一次潜在访问。
     * - next_ 设为 nullptr：初始时不指向任何后继节点。
     * - inFreqList_ 设为 false：初始状态不在任何频率管理列表中。
     */
    template <typename Key, typename Value>
    ArcNode<Key, Value>::ArcNode()
        : accessCount_(1), 
          next_(nullptr),
          inFreqList_(false)
    {
    }

    /**
     * @brief 带参构造函数
     * @param key 缓存键
     * @param value 缓存值
     * 
     * 用于创建一个存储实际数据的节点。
     */
    template <typename Key, typename Value>
    ArcNode<Key, Value>::ArcNode(Key key, Value value)
        : key_(key),
          value_(value),
          accessCount_(1),
          next_(nullptr),
          inFreqList_(false)
    {
    }

    /**
     * @brief 获取当前节点的键
     * @return Key 缓存键
     */
    template <typename Key, typename Value>
    Key ArcNode<Key, Value>::getKey() const
    {
        return key_;
    }

    /**
     * @brief 获取当前节点的值
     * @return Value 缓存值
     */
    template <typename Key, typename Value>
    Value ArcNode<Key, Value>::getValue() const
    {
        return value_;
    }

    /**
     * @brief 获取节点被访问的总次数
     * @return size_t 访问计数
     */
    template <typename Key, typename Value>
    size_t ArcNode<Key, Value>::getAccessCount() const
    {
        return accessCount_;
    }

    /**
     * @brief 更新当前节点的值
     * @param value 新的缓存值
     */
    template <typename Key, typename Value>
    void ArcNode<Key, Value>::setValue(const Value &value)
    {
        value_ = value;
    }

    /**
     * @brief 增加节点的访问计数
     * 
     * 通常在缓存命中（Cache Hit）时调用。
     * 访问计数的增加可能触发节点在 ARC 内部列表（如从 L1 移到 L2）之间的晋升。
     */
    template <typename Key, typename Value>
    void ArcNode<Key, Value>::incrementAccessCount()
    {
        ++accessCount_;
    }

    /**
     * @brief 手动设置访问计数
     * @param count 目标计数值
     * 
     * 用于在特定的策略调整或状态恢复时精确控制节点的权重。
     */
    template <typename Key, typename Value>
    void ArcNode<Key, Value>::setAccessCount(size_t count)
    {
        accessCount_ = count;
    }
}