#pragma once

#include <list>
#include <map>
#include <mutex>
#include <unordered_map>

#include "ArcCacheNode.h"

namespace CacheSys
{
    template <typename Key, typename Value>
    class ArcLfuPart
    {
    public:
        using NodeType = ArcNode<Key, Value>;
        using NodePtr = std::shared_ptr<NodeType>;
        using NodeMap = std::unordered_map<Key, NodePtr>;
        using FreqMap = std::map<size_t, std::list<NodePtr>>;

        ArcLfuPart(size_t capacity, size_t transformThreshold);

        bool put(Key key, Value value);
        bool get(Key key, Value &value);
        bool contains(Key key);
        bool consumeGhostEntry(Key key);

        // Backward compatibility wrappers.
        bool contain(Key key) { return contains(key); }
        bool checkGhost(Key key) { return consumeGhostEntry(key); }

        void increaseCapacity();
        bool decreaseCapacity();

    private:
        void initializeLists();
        bool addNewNode(const Key &key, const Value &value);
        bool updateExistingNode(NodePtr node, const Value &value);

        void updateNodeFrequency(NodePtr node);
        void evictLeastFrequent();

        void removeFromList(NodePtr node);
        void addToGhost(NodePtr node);
        void removeOldestGhost();

    private:
        size_t capacity_;
        size_t ghostCapacity_;
        size_t transformThreshold_;
        size_t minFreq_;
        std::mutex mutex_;

        NodeMap mainCache_;
        NodeMap ghostCache_;
        FreqMap freqMap_;

        NodePtr ghostHead_;
        NodePtr ghostTail_;
    };
}

#include "../../src/detail/ArcLfuPart.tpp"
