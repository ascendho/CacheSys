#pragma once

namespace CacheSys
{
    template <typename Key, typename Value>
    ArcNode<Key, Value>::ArcNode()
        : accessCount_(1), // 访问计数初始化为1（而非0），避免首次访问后计数仍为0的逻辑漏洞
                    next_(nullptr),
                    inFreqList_(false)
    {
    }

    template <typename Key, typename Value>
    ArcNode<Key, Value>::ArcNode(Key key, Value value)
        : key_(key),
          value_(value),
          accessCount_(1),
            next_(nullptr),
            inFreqList_(false)
    {
    }

    template <typename Key, typename Value>
    Key ArcNode<Key, Value>::getKey() const
    {
        return key_;
    }

    template <typename Key, typename Value>
    Value ArcNode<Key, Value>::getValue() const
    {
        return value_;
    }

    template <typename Key, typename Value>
    size_t ArcNode<Key, Value>::getAccessCount() const
    {
        return accessCount_;
    }

    template <typename Key, typename Value>
    void ArcNode<Key, Value>::setValue(const Value &value)
    {
        value_ = value;
    }

    /**
     * @details 每次访问节点时调用（如缓存命中时），用于LFU策略统计节点的访问热度
     */
    template <typename Key, typename Value>
    void ArcNode<Key, Value>::incrementAccessCount()
    {
        // 访问计数加1，操作原子性由上层调用保证
        ++accessCount_;
    }

    /**
     * @brief 手动设置访问计数的实现
     * @param count 目标访问计数
     * @details 用于缓存策略调整（如节点迁移、计数重置），允许直接覆盖原有计数
     */
    template <typename Key, typename Value>
    void ArcNode<Key, Value>::setAccessCount(size_t count)
    {
        // 直接赋值覆盖原有访问计数
        accessCount_ = count;
    }
}