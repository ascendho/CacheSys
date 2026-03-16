#pragma once

namespace CacheSys
{
    template <typename Key, typename Value>
    ArcNode<Key, Value>::ArcNode() : accessCount_(1), next_(nullptr)
    {
    }

    template <typename Key, typename Value>
    ArcNode<Key, Value>::ArcNode(Key key, Value value)
        : key_(key), value_(value), accessCount_(1), next_(nullptr)
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

    template <typename Key, typename Value>
    void ArcNode<Key, Value>::incrementAccessCount()
    {
        ++accessCount_;
    }

    template <typename Key, typename Value>
    void ArcNode<Key, Value>::setAccessCount(size_t count)
    {
        accessCount_ = count;
    }
}
