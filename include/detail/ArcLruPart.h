#pragma once

#include <mutex>
#include <unordered_map>

#include "ArcCacheNode.h"

namespace CacheSys
{
    template <typename Key, typename Value>
    class ArcLruPart
    {
    public:
        using NodeType = ArcNode<Key, Value>;
        using NodePtr = std::shared_ptr<NodeType>;
        using NodeMap = std::unordered_map<Key, NodePtr>;

        ArcLruPart(size_t capacity, size_t transformThreshold);

        bool put(Key key, Value value);
        bool get(Key key, Value &value, bool &shouldTransform);
        bool checkGhost(Key key);

        void increaseCapacity();
        bool decreaseCapacity();

    private:
        void initializeLists();
        bool addNewNode(const Key &key, const Value &value);
        bool updateNodeAccess(NodePtr node, const Value *value);

        void moveToFront(NodePtr node);
        void addToFront(NodePtr node);
        void removeFromList(NodePtr node);

        void evictLeastRecent();
        void addToGhost(NodePtr node);
        void removeOldestGhost();

    private:
        size_t capacity_;
        size_t ghostCapacity_;
        size_t transformThreshold_;
        std::mutex mutex_;

        NodeMap mainCache_;
        NodeMap ghostCache_;

        NodePtr mainHead_;
        NodePtr mainTail_;
        NodePtr ghostHead_;
        NodePtr ghostTail_;
    };
}

#include "../../src/detail/ArcLruPart.tpp"
