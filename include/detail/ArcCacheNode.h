#pragma once

#include <memory>

namespace CacheSys
{
    template <typename Key, typename Value>
    class ArcLruPart;

    template <typename Key, typename Value>
    class ArcLfuPart;

    template <typename Key, typename Value>
    class ArcNode
    {
    public:
        ArcNode();
        ArcNode(Key key, Value value);

        Key getKey() const;
        Value getValue() const;
        size_t getAccessCount() const;

        void setValue(const Value &value);
        void incrementAccessCount();
        void setAccessCount(size_t count);

    private:
        Key key_;
        Value value_;
        size_t accessCount_;
        std::weak_ptr<ArcNode<Key, Value>> prev_;
        std::shared_ptr<ArcNode<Key, Value>> next_;

        friend class ArcLruPart<Key, Value>;
        friend class ArcLfuPart<Key, Value>;
    };
}

#include "../../src/detail/ArcCacheNode.tpp"
