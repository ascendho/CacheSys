#pragma once

#include <cmath>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "CachePolicy.h"

namespace CacheSys
{
    // 前向声明：FreqList友元依赖LfuCache
    template <typename Key, typename Value>
    class LfuCache;

    /**
     * @brief LFU频率桶：管理同一访问频率的双向链表节点
     * @tparam Key 缓存键类型
     * @tparam Value 缓存值类型
     */
    template <typename Key, typename Value>
    class FreqList
    {
    private:
        /**
         * @brief LFU双向链表内部节点
         * 存储键、值、访问频率及前后指针，弱指针防循环引用
         */
        struct Node
        {
            int freq;                   // 节点访问频率
            Key key;                    // 缓存键
            Value value;                // 缓存值
            std::weak_ptr<Node> pre;    // 前驱弱指针
            std::shared_ptr<Node> next; // 后继共享指针

            Node();
            Node(Key key, Value value);
        };

        using NodePtr = std::shared_ptr<Node>;

    public:
        explicit FreqList(int n); // 构造：绑定指定频率n

        bool isEmpty() const;          // 判断频率桶是否为空
        void addNode(NodePtr node);    // 添加节点到桶尾部
        void removeNode(NodePtr node); // 从桶中移除指定节点
        NodePtr getFirstNode() const;  // 获取桶头部首个节点

    private:
        int freq_;     // 当前频率桶对应的频率值
        NodePtr head_; // 链表头哨兵
        NodePtr tail_; // 链表尾哨兵

        friend class LfuCache<Key, Value>;
    };

    /**
     * @brief 基础LFU(最不经常使用)缓存，继承通用缓存接口
     * 核心结构：哈希表快速查找 + 频率桶分组节点 + 最小频率快速淘汰
     */
    template <typename Key, typename Value>
    class LfuCache : public CachePolicy<Key, Value>
    {
    public:
        using Node = typename FreqList<Key, Value>::Node;
        using NodePtr = std::shared_ptr<Node>;
        using NodeMap = std::unordered_map<Key, NodePtr>;

        explicit LfuCache(int capacity, int maxAverageNum = 1000000);
        ~LfuCache() override;

        void put(Key key, Value value) override;  // 写入/更新缓存
        bool get(Key key, Value &value) override; // 读取缓存(输出参数)
        Value get(Key key) override;              // 便捷读取缓存

        void purge(); // 清空所有缓存与频率桶

    private:
        void putInternal(Key key, Value value);       // 写入核心逻辑
        void getInternal(NodePtr node, Value &value); // 命中更新频率
        void evictOneEntry();                         // 淘汰最小频率首个节点

        void removeFromFreqList(NodePtr node); // 从原频率桶移除节点
        void addToFreqList(NodePtr node);      // 将节点加入新频率桶

        void addFreqNum();              // 频率统计累加
        void decreaseFreqNum(int num);  // 频率统计衰减
        void handleFrequencyOverflow(); // 处理频率数值溢出

        void updateMinFreq();  // 更新全局最小频率
        void clearFreqLists(); // 释放所有动态频率桶

    private:
        int capacity_;                                                   // 缓存最大容量
        int minFreq_;                                                    // 当前全局最小访问频率
        int maxAverageNum_;                                              // 最大平均访问阈值(防溢出)
        int curAverageNum_;                                              // 当前平均访问次数
        int curTotalNum_;                                                // 累计总访问次数
        std::mutex mutex_;                                               // 全局互斥锁，保证线程安全
        NodeMap nodeMap_;                                                // 键→节点哈希表，O(1)查找
        std::unordered_map<int, std::unique_ptr<FreqList<Key, Value>>> freqToFreqList_; // 频率→频率桶映射
    };

    /**
     * @brief 分片LFU缓存：哈希分散Key到多个独立LFU分片
     * 降低全局锁竞争，提升高并发读写性能
     */
    template <typename Key, typename Value>
    class ShardedLfuCache
    {
    public:
        ShardedLfuCache(size_t capacity, int sliceNum, int maxAverageNum = 10);

        void put(Key key, Value value);  // 写入缓存
        bool get(Key key, Value &value); // 读取缓存
        Value get(Key key);              // 便捷读取
        void purge();                    // 清空所有分片缓存

    private:
        size_t hashKey(const Key &key) const; // 计算Key所属分片索引

    private:
        size_t capacity_;                                                   // 缓存总容量
        int sliceNum_;                                                      // 分片数量
        std::vector<std::unique_ptr<LfuCache<Key, Value>>> lfuSliceCaches_; // LFU分片数组
    };

    // 类型别名：向后兼容旧类名
    template <typename Key, typename Value>
    using HashLfuCache = ShardedLfuCache<Key, Value>;
}

#include "../src/LfuCache.tpp"