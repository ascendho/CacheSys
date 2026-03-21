#pragma once

#include <cmath>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "CachePolicy.h"

namespace CacheSys
{
    // 前向声明：LruNode 中将 LruCache 声明为友元，需先告知编译器该类存在
    template <typename Key, typename Value>
    class LruCache;

    /**
     * @brief LRU 缓存的双向链表节点类
     * @details 每个节点封装键值对、访问计数，以及维护链表顺序的前驱/后继指针，
     *          是 LRU 链表的基本组成单元
     */
    template <typename Key, typename Value>
    class LruNode
    {
    public:
        LruNode(Key key, Value value);

        Key getKey() const;

        Value getValue() const;

        void setValue(const Value &value);

        size_t getAccessCount() const;

        void incrementAccessCount();

    private:
        Key key_;
        Value value_;
        size_t accessCount_;
        std::weak_ptr<LruNode<Key, Value>> prev_;
        std::shared_ptr<LruNode<Key, Value>> next_;

        // 友元声明：允许 LruCache 直接操作私有指针（prev_/next_），简化链表操作
        friend class LruCache<Key, Value>;
    };

    /**
     * @brief 基础 LRU（最近最少使用）缓存实现
     * @details 继承自 CachePolicy 抽象接口，实现核心的 put/get/remove 方法，
     *          核心结构为“哈希表 + 双向链表”：
     *          - 哈希表（nodeMap_）：O(1) 时间复杂度查找节点
     *          - 双向链表（带哨兵节点）：O(1) 时间复杂度调整节点顺序、淘汰节点
     *          线程安全：通过 mutex_ 保证所有操作的原子性
     */
    template <typename Key, typename Value>
    class LruCache : public CachePolicy<Key, Value>
    {
    public:
        using LruNodeType = LruNode<Key, Value>;          // LRU 节点类型
        using NodePtr = std::shared_ptr<LruNodeType>;     // 节点智能指针类型
        using NodeMap = std::unordered_map<Key, NodePtr>; // 键到节点的哈希表类型

        /**
         * @brief 构造函数：初始化缓存容量和哨兵链表
         * @param capacity 缓存最大容量（节点数量上限）
         */
        explicit LruCache(int capacity);

        /**
         * @brief 析构函数：默认实现
         * @details 依赖智能指针自动释放节点，无需手动清理内存
         */
        ~LruCache() override = default;

        /**
         * @brief 写入/更新缓存项（重写抽象接口）
         * @details 逻辑：
         *          1. 若 key 已存在：更新值并将节点移到“最近使用”位置
         *          2. 若 key 不存在：先淘汰（若满）再插入新节点到链表尾部
         */
        void put(Key key, Value value) override;

        /**
         * @brief 读取缓存项（重写抽象接口，输出参数版）
         * @return bool - true：命中（更新节点到最近使用位置），false：未命中
         * @details 命中时自动更新统计（hits++），未命中更新 misses++
         */
        bool get(Key key, Value &value) override;

        /**
         * @brief 便捷读取接口（重写抽象接口，无输出参数版）
         * @return Value - 命中返回对应值，未命中返回 Value 默认构造值
         * @details 封装 get(Key, Value&)，简化调用逻辑
         */
        Value get(Key key) override;

        /**
         * @brief 显式删除指定缓存项（重写抽象接口）
         * @details 从哈希表和双向链表中同时移除节点，释放内存
         */
        void remove(Key key) override;

    private:
        /**
         * @brief 初始化带哨兵节点的双向链表
         * @details 哨兵节点（dummyHead_/dummyTail_）简化链表操作：
         *          - 无需处理“头/尾节点为空”的边界条件
         *          - 插入/删除节点时逻辑统一
         */
        void initializeList();

        /**
         * @brief 更新已存在的节点
         * @details 1. 更新节点值 2. 将节点移到链表尾部（最近使用位置）
         */
        void updateExistingNode(NodePtr node, const Value &value);

        /**
         * @brief 添加新节点到缓存
         * @details 1. 若缓存已满，先淘汰最久未使用节点 2. 创建新节点 3. 插入到链表尾部 + 哈希表
         */
        void addNewNode(const Key &key, const Value &value);

        /**
         * @brief 将节点移动到链表尾部（最近使用位置）
         * @details 1. 从原位置摘除节点 2. 插入到尾部，是 LRU 核心操作
         */
        void moveToMostRecent(NodePtr node);

        /**
         * @brief 从双向链表中摘除指定节点
         * @details 仅修改前驱/后继指针，不删除节点（由智能指针管理生命周期）
         */
        void removeNode(NodePtr node);

        /**
         * @brief 将节点插入到链表尾部（哨兵尾节点前）
         */
        void insertNode(NodePtr node);

        /**
         * @brief 淘汰最久未使用的节点
         * @details 1. 移除链表头部有效节点（dummyHead_->next_）
         *          2. 从哈希表中删除对应键 3. 更新淘汰统计（evictions++）
         */
        void evictLeastRecent();

    private:
        int capacity_;     // 缓存最大容量：超过则触发淘汰
        NodeMap nodeMap_;  // 哈希表：键 -> 节点指针，O(1) 查找
        std::mutex mutex_; // 互斥锁：保证所有操作线程安全，避免并发竞争
        NodePtr dummyHead_;
        NodePtr dummyTail_;
    };

    /**
     * @brief LRU-K 缓存实现（LRU 扩展版）
     * @details 继承自基础 LruCache，核心优化：增加“访问次数阈值 K”，
     *          只有被访问达到 K 次的冷数据才进入主缓存，
     *          避免一次性冷数据（如爬虫、临时查询）污染主缓存，提升缓存命中率
     * 核心结构：
     * - 主缓存：基础 LruCache 实例（存储高频访问数据）
     * - 历史缓存（historyList_）：存储访问次数不足 K 的数据的访问计数
     * - 历史值映射（historyValueMap_）：存储历史缓存的实际值
     */

    template <typename Key, typename Value>
    class LruKCache : public LruCache<Key, Value>
    {
    public:
        /**
         * @brief 构造函数：初始化 LRU-K 缓存
         * @param capacity 主缓存容量
         * @param historyCapacity 历史缓存容量
         * @param k 访问次数阈值（达到 K 次进入主缓存）
         */
        LruKCache(int capacity, int historyCapacity, int k);

        /**
         * @brief 读取缓存项（重写，适配 LRU-K 逻辑）
         * @return Value - 命中返回对应值，未命中返回默认值
         * @details 逻辑：
         *          1. 先查主缓存：命中则更新 LRU 顺序
         *          2. 未命中则查历史缓存：更新访问计数，达到 K 则移入主缓存
         *          3. 均未命中则返回默认值
         */
        Value get(Key key);

        /**
         * @brief 写入缓存项（重写，适配 LRU-K 逻辑）
         * @details 逻辑：
         *          1. 主缓存存在则更新
         *          2. 历史缓存存在则更新值
         *          3. 均不存在则加入历史缓存，计数初始化为 1
         */
        void put(Key key, Value value);

    private:
        int k_;                                              // LRU-K 的阈值 K：访问次数达到 K 才进入主缓存
        std::unique_ptr<LruCache<Key, size_t>> historyList_; // 历史缓存：存储键 -> 访问次数
        std::unordered_map<Key, Value> historyValueMap_;     // 历史缓存值映射：存储未达标数据的实际值
        std::mutex lruKMutex_;                               // 保护 historyList_/historyValueMap_ 复合操作
    };

    /**
     * @brief 分片版 LRU 缓存（Sharded LRU）
     * @details 核心优化：将缓存拆分为多个独立的 LRU 分片，每个分片有自己的锁，
     *          降低全局锁的竞争概率，提升高并发场景下的性能
     * 核心逻辑：
     * - 通过哈希函数将 key 映射到指定分片
     * - 所有操作（put/get）仅锁定对应分片，而非全局锁
     */
    template <typename Key, typename Value>
    class ShardedLruCache
    {
    public:
        /**
         * @brief 构造函数：初始化分片 LRU
         * @param capacity 总缓存容量（平均分配到各分片）
         * @param sliceNum 分片数量（建议为 2^n，提升哈希分布均匀性）
         */
        ShardedLruCache(size_t capacity, int sliceNum);

        /**
         * @brief 写入缓存项
         * @details 1. 计算 key 所属分片 2. 调用对应分片的 put 方法
         */
        void put(Key key, Value value);

        /**
         * @brief 读取缓存项（输出参数版）
         * @return bool - true：命中，false：未命中
         * @details 1. 计算 key 所属分片 2. 调用对应分片的 get 方法
         */
        bool get(Key key, Value &value);

        /**
         * @brief 便捷读取接口
         * @return Value - 命中返回对应值，未命中返回默认值
         */
        Value get(Key key);

    private:
        /**
         * @brief 哈希函数：计算 key 所属的分片索引
         * @return size_t - 分片索引（范围 [0, sliceNum_-1]）
         * @details 基于 key 的哈希值取模，保证分布均匀
         */
        size_t hashKey(const Key &key) const;

    private:
        size_t capacity_; // 总缓存容量
        int sliceNum_;    // 分片数量

        // 分片数组：每个元素是一个独立的 LruCache 实例（unique_ptr 管理生命周期）
        std::vector<std::unique_ptr<LruCache<Key, Value>>> lruSliceCaches_;
    };

    /**
     * @brief 类型别名：向后兼容旧类名
     * @details 避免现有调用代码因类名变更（ShardedLruCache 替代 HashLruCaches）而直接失效，
     *          是工程上的兼容性设计技巧
     */
    template <typename Key, typename Value>
    using HashLruCaches = ShardedLruCache<Key, Value>;
}

#include "../src/LruCache.tpp"