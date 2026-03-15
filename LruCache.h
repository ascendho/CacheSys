#pragma once

// 标准库头文件：提供基础数据结构和线程安全支持
#include <cstring>       // 内存操作
#include <list>          // 链表容器
#include <memory>        // 智能指针（shared_ptr/weak_ptr，管理节点生命周期）
#include <mutex>         // 互斥锁（std::mutex/std::lock_guard，保证线程安全）
#include <unordered_map> // 哈希表（O(1)查找节点）

// 缓存策略基类：定义缓存操作的统一接口
#include "CachePolicy.h"

/**
 * @namespace CacheSys
 * @brief 缓存系统核心命名空间，封装各类缓存替换策略的实现
 * @note 本命名空间下的所有缓存实现均保证线程安全
 */
namespace CacheSys
{
    // 前向声明：LruCache需作为LruNode的友元类，提前声明避免编译依赖
    template <typename Key, typename Value>
    class LruCache;

    /**
     * @class LruNode
     * @brief LRU缓存的节点结构，存储键值对、访问次数及双向链表指针
     * @tparam Key 缓存键类型（需支持哈希运算和相等比较，满足unordered_map键要求）
     * @tparam Value 缓存值类型（支持任意可拷贝/赋值的类型）
     * @note 采用weak_ptr+shared_ptr组合解决双向链表的循环引用问题
     */
    template <typename Key, typename Value>
    class LruNode
    {
    private:
        Key key_;                                   // 缓存键（唯一标识节点）
        Value value_;                               // 缓存值
        size_t accessCount_;                        // 访问次数（预留LFU扩展，LRU仅初始化为1）
        std::weak_ptr<LruNode<Key, Value>> prev_;   // 前驱节点（weak_ptr：打破循环引用，不持有所有权）
        std::shared_ptr<LruNode<Key, Value>> next_; // 后继节点（shared_ptr：持有所有权，管理生命周期）

    public:
        /**
         * @brief 构造函数：初始化键值对，访问次数默认设为1
         * @param key 缓存键
         * @param value 缓存值
         */
        LruNode(Key key, Value value) : key_(key), value_(value), accessCount_(1) {}

        /**
         * @brief 获取缓存键（只读）
         * @return Key 缓存键的拷贝
         */
        Key getKey() const { return key_; }

        /**
         * @brief 获取缓存值（只读）
         * @return Value 缓存值的拷贝
         */
        Value getValue() const { return value_; }

        /**
         * @brief 更新缓存值
         * @param value 新的缓存值
         */
        void setValue(const Value &value) { value_ = value; }

        /**
         * @brief 获取节点访问次数（只读）
         * @return size_t 访问次数
         */
        size_t getAccessCount() const { return accessCount_; }

        /**
         * @brief 访问次数自增（每次访问节点时调用）
         */
        void incrementAccessCount() { ++accessCount_; }

        // 友元声明：LruCache需直接操作链表指针，保证链表操作的封装性
        friend class LruCache<Key, Value>;
    };

    /**
     * @class LruCache
     * @brief 线程安全的LRU（最近最少使用）缓存实现类
     * @tparam Key 缓存键类型
     * @tparam Value 缓存值类型
     * @extends CachePolicy<Key, Value> 继承缓存策略接口，统一操作规范
     * @核心实现：双向链表（维护访问顺序） + 哈希表（O(1)查找） + 互斥锁（线程安全）
     */
    template <typename Key, typename Value>
    class LruCache : public CachePolicy<Key, Value>
    {
    public:
        // 类型别名：简化代码书写，提升可读性和可维护性
        using LruNodeType = LruNode<Key, Value>;          // LRU节点类型，给模板类 LruNode 指定类型参数
        using NodePtr = std::shared_ptr<LruNodeType>;     // 节点智能指针类型
        using NodeMap = std::unordered_map<Key, NodePtr>; // 键到节点的哈希表类型

        /**
         * @brief 构造函数：初始化缓存容量和双向链表（虚拟头尾节点）
         * @param capacity 缓存最大容量（超过容量触发LRU淘汰）
         */
        LruCache(int capacity) : capacity_(capacity)
        {
            initializeList(); // 初始化双向链表的虚拟头尾节点
        }

        /**
         * @brief 析构函数：默认实现
         * @note 智能指针自动管理节点内存，无需手动释放；互斥锁自动销毁
         */
        ~LruCache() override = default;

        /**
         * @brief 添加/更新缓存项（线程安全）
         * @param key 缓存键
         * @param value 缓存值
         * @override 重写基类CachePolicy的put接口
         * @note 容量≤0时直接返回，避免无效操作
         */
        void put(Key key, Value value) override
        {
            if (capacity_ <= 0)
                return;

            // 加锁：std::lock_guard基于RAII，作用域结束自动释放锁，防止死锁
            std::lock_guard<std::mutex> lock(mutex_);

            // 查找缓存是否已存在
            auto it = nodeMap_.find(key);
            if (it != nodeMap_.end())
            {
                // 缓存存在：更新值并标记为“最近使用”
                updateExistingNode(it->second, value);
                return;
            }

            // 缓存不存在：添加新节点（容量满则先淘汰LRU节点）
            addNewNode(key, value);
        }

        /**
         * @brief 获取缓存项（线程安全，通过引用返回值）
         * @param key 缓存键
         * @param value 输出参数：找到则赋值，未找到保持原值
         * @return bool 找到返回true，未找到返回false
         * @override 重写基类CachePolicy的get接口
         */
        bool get(Key key, Value &value) override
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // 哈希表O(1)查找节点
            auto it = nodeMap_.find(key);

            if (it != nodeMap_.end())
            {
                // 找到缓存：标记为“最近使用”（移到链表尾部）
                moveToMostRecent(it->second);
                // 赋值输出参数
                value = it->second->getValue();
                return true;
            }

            // 未找到缓存
            return false;
        }

        /**
         * @brief 重载get方法：直接返回缓存值（线程安全）
         * @param key 缓存键
         * @return Value 找到返回对应值，未找到返回Value的默认构造值
         * @note 复杂类型（如std::string）建议使用带引用的get方法，避免默认构造开销
         */
        Value get(Key key) override
        {
            Value value{};   // 默认构造值（避免memset破坏复杂类型结构）
            get(key, value); // 复用带引用的get逻辑
            return value;
        }

        /**
         * @brief 删除指定缓存项（线程安全）
         * @param key 要删除的缓存键
         * @note 先从链表移除节点，再从哈希表删除映射，保证数据一致性
         */
        void remove(Key key)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = nodeMap_.find(key);
            if (it != nodeMap_.end())
            {
                // 从链表移除节点
                removeNode(it->second);
                // 从哈希表删除键值对
                nodeMap_.erase(it);
            }
        }

    private:
        /**
         * @brief 初始化双向链表：创建虚拟头尾节点并建立连接
         * @note 虚拟节点避免处理“头/尾节点为空”的边界情况，简化链表操作
         */
        void initializeList()
        {
            // 创建虚拟头/尾节点（默认构造键值，仅作为链表边界）
            dummyHead_ = std::make_shared<LruNodeType>(Key(), Value());
            dummyTail_ = std::make_shared<LruNodeType>(Key(), Value());
            // 初始化链表：头节点→尾节点，尾节点→头节点
            dummyHead_->next_ = dummyTail_;
            dummyTail_->prev_ = dummyHead_;
        }

        /**
         * @brief 更新已存在的节点：更新值 + 标记为最近使用
         * @param node 要更新的节点指针
         * @param value 新的缓存值
         */
        void updateExistingNode(NodePtr node, const Value &value)
        {
            node->setValue(value);  // 更新节点值
            moveToMostRecent(node); // 移到链表尾部（最近使用位置）
        }

        /**
         * @brief 添加新节点到缓存（容量满时先淘汰LRU节点）
         * @param key 新缓存键
         * @param value 新缓存值
         */
        void addNewNode(const Key &key, const Value &value)
        {
            // 缓存容量已满：淘汰“最近最少使用”的节点（链表头部）
            if (nodeMap_.size() >= capacity_)
            {
                evictLeastRecent();
            }

            // 创建新节点
            NodePtr newNode = std::make_shared<LruNodeType>(key, value);
            // 插入到链表尾部（最近使用位置）
            insertNode(newNode);
            // 哈希表添加键值映射
            nodeMap_[key] = newNode;
        }

        /**
         * @brief 将节点移到“最近使用”位置（链表尾部，虚拟尾节点前）
         * @param node 要移动的节点指针
         * @note 先移除节点，再重新插入到尾部，保证链表顺序正确
         */
        void moveToMostRecent(NodePtr node)
        {
            removeNode(node); // 从原位置移除节点
            insertNode(node); // 插入到尾部
        }

        /**
         * @brief 从双向链表中移除指定节点
         * @param node 要移除的节点指针
         * @note 处理weak_ptr的lock()操作，确保前驱节点未被释放
         */
        void removeNode(NodePtr node)
        {
            // 检查前驱节点是否有效（未过期）且后继节点存在
            if (!node->prev_.expired() && node->next_)
            {
                // weak_ptr转shared_ptr（lock()返回空表示前驱已释放）
                auto prev = node->prev_.lock();
                // 调整链表指针：前驱→后继
                prev->next_ = node->next_;
                // 调整链表指针：后继→前驱
                node->next_->prev_ = prev;
                // 清空当前节点的后继，彻底断开连接
                node->next_ = nullptr;
            }
        }

        /**
         * @brief 将节点插入到双向链表的尾部（虚拟尾节点前）
         * @param node 要插入的节点指针
         */
        void insertNode(NodePtr node)
        {
            node->next_ = dummyTail_;               // 新节点的后继指向虚拟尾节点
            node->prev_ = dummyTail_->prev_;        // 新节点的前驱指向尾节点的原前驱
            dummyTail_->prev_.lock()->next_ = node; // 尾节点原前驱的后继指向新节点
            dummyTail_->prev_ = node;               // 尾节点的前驱指向新节点
        }

        /**
         * @brief 驱逐“最近最少使用”的节点（链表头部第一个有效节点）
         * @note 淘汰后同步删除哈希表中的映射，保证数据一致性
         */
        void evictLeastRecent()
        {
            // 虚拟头节点的后继是“最近最少使用”的节点
            NodePtr leastRecent = dummyHead_->next_;
            // 从链表移除
            removeNode(leastRecent);
            // 从哈希表删除
            nodeMap_.erase(leastRecent->getKey());
        }

    private:
        int capacity_;      // 缓存最大容量（核心配置）
        NodeMap nodeMap_;   // 哈希表：键→节点指针（O(1)查找）
        std::mutex mutex_;  // 互斥锁：保证所有操作的线程安全
        NodePtr dummyHead_; // 虚拟头节点：简化链表头部操作
        NodePtr dummyTail_; // 虚拟尾节点：简化链表尾部操作
    };

    /**
     * @class LruKCache
     * @brief LRU-K 缓存实现类，继承自基础 LRU 缓存，解决传统 LRU 缓存污染、访问模式不敏感问题
     * @tparam Key 缓存键类型（需支持哈希和相等比较）
     * @tparam Value 缓存值类型
     * @extends LruCache<Key, Value> 继承基础 LRU 作为「主缓存」，存储达到 K 次访问的热点数据
     * @核心设计 LRU-K 核心逻辑：
     *          1. 分「历史队列」和「主缓存」两层，过滤一次性冷数据；
     *          2. 数据需被访问 ≥K 次，才会从历史队列进入主缓存；
     *          3. 一次性冷数据（访问次数 <K）仅存于历史队列，满了直接淘汰，不污染主缓存。
     */
    template <typename Key, typename Value>
    class LruKCache : public LruCache<Key, Value>
    {
    public:
        /**
         * @brief 构造函数：初始化 LRU-K 缓存的核心参数
         * @param capacity 主缓存（基础 LRU）的最大容量（存储热点数据）
         * @param historyCapacity 历史队列的最大容量（存储访问次数 <K 的数据）
         * @param k 访问阈值：数据需被访问 ≥k 次才进入主缓存（通常 k=2，即 2Q 策略）
         * @初始化说明：
         *  1. LruCache<Key, Value>(capacity)：初始化父类（主缓存）的容量；
         *  2. historyList_：创建历史队列（用 LRU 缓存存储「键→访问次数」，容量为 historyCapacity）；
         *  3. k_：赋值访问阈值 k。
         */
        LruKCache(int capacity, int historyCapacity, int k)
            : LruCache<Key, Value>(capacity),                                         // 初始化父类（主LRU缓存）
              historyList_(std::make_unique<LruCache<Key, size_t>>(historyCapacity)), // 历史访问次数队列
              k_(k)                                                                   // 设置K次访问阈值
        {
        }

        /**
         * @brief 获取缓存值（重载父类get，实现LRU-K访问逻辑）
         * @param key 缓存键
         * @return Value 找到则返回对应值，未找到返回Value的默认构造值
         * @核心逻辑：
         *  1. 先查主缓存，若存在则直接返回；
         *  2. 无论主缓存是否命中，都更新该键在历史队列的访问次数；
         *  3. 若历史访问次数 ≥K，将数据从历史队列移入主缓存；
         *  4. 未命中则返回默认值。
         */
        Value get(Key key)
        {
            Value value{}; // 初始化返回值（默认构造）
            // 第一步：查询主缓存（基础LRU）是否存在该键
            bool inMainCache = LruCache<Key, Value>::get(key, value);

            // 第二步：更新历史队列的访问次数（即使主缓存命中，也要更新访问次数）
            size_t historyCount = historyList_->get(key); // 获取当前历史访问次数（默认0）
            historyCount++;                               // 访问次数+1
            historyList_->put(key, historyCount);         // 写回历史队列

            // 主缓存命中：直接返回值（已更新历史访问次数）
            if (inMainCache)
                return value;

            // 第三步：历史访问次数 ≥K → 将数据移入主缓存
            if (historyCount >= k_)
            {
                // 查找历史值映射表中的值
                auto it = historyValueMap_.find(key);
                if (it != historyValueMap_.end())
                {
                    Value storedValue = it->second; // 获取历史队列中存储的值

                    // 从历史队列移除该键（次数和值都删除）
                    historyList_->remove(key);
                    historyValueMap_.erase(it);

                    // 将数据插入主缓存（基础LRU）
                    LruCache<Key, Value>::put(key, storedValue);

                    // 返回历史队列中存储的值
                    return storedValue;
                }
            }

            // 主缓存未命中 + 历史访问次数 <K → 返回默认值
            return value;
        }

        /**
         * @brief 添加/更新缓存项（重载父类put，实现LRU-K写入逻辑）
         * @param key 缓存键
         * @param value 缓存值
         * @核心逻辑：
         *  1. 若主缓存已存在，直接更新值并返回；
         *  2. 若主缓存不存在，更新历史队列的访问次数；
         *  3. 将值存入历史值映射表；
         *  4. 若历史访问次数 ≥K，将数据移入主缓存，并清理历史队列/映射表。
         */
        void put(Key key, Value value)
        {
            Value existingValue{};
            // 第一步：查询主缓存是否存在该键
            bool inMainCache = LruCache<Key, Value>::get(key, existingValue);

            // 主缓存存在：直接更新值，无需走历史队列
            if (inMainCache)
            {
                LruCache<Key, Value>::put(key, value);
                return;
            }

            // 第二步：主缓存不存在 → 更新历史队列的访问次数
            size_t historyCount = historyList_->get(key); // 获取当前历史访问次数（默认0）
            historyCount++;                               // 访问次数+1
            historyList_->put(key, historyCount);         // 写回历史队列

            // 第三步：将值存入历史值映射表（记录未达到K次访问的数据值）
            historyValueMap_[key] = value;

            // 第四步：历史访问次数 ≥K → 移入主缓存，清理历史数据
            if (historyCount >= k_)
            {
                // 修正笔误：historylist_ → historyList_（统一命名规范）
                historyList_->remove(key);             // 从历史次数队列移除
                historyValueMap_.erase(key);           // 从历史值映射表移除
                LruCache<Key, Value>::put(key, value); // 插入主缓存
            }
        }

    private:
        int k_; ///< LRU-K 的核心阈值：数据需被访问 ≥k_ 次才进入主缓存（通常k=2）
        /// 历史访问次数队列：存储「键→访问次数」，容量有限，满了按LRU淘汰（过滤一次性冷数据）
        /// unique_ptr 独占所有权：确保历史队列的生命周期和LruKCache一致，自动释放
        std::unique_ptr<LruCache<Key, size_t>> historyList_;
        /// 历史值映射表：存储未达到K次访问的数据值（historyList_仅存次数，需单独存值）
        std::unordered_map<Key, Value> historyValueMap_;
    };

    /**
     * @class HashLruCaches
     * @brief 分片LRU（HashLRU）缓存实现类，解决基础LRU全局锁导致的高并发性能瓶颈
     * @tparam Key 缓存键类型（需支持std::hash哈希运算和相等比较）
     * @tparam Value 缓存值类型
     * @核心设计思路：
     *  1. 把全局LRU拆分为多个独立的LRU分片（Slice），每个分片有自己的锁和缓存空间；
     *  2. 按key的哈希值取模，将不同key路由到不同分片，实现「分片级锁隔离」；
     *  3. 高并发下不同分片的操作可并行执行，大幅降低锁冲突，提升吞吐量。
     */
    template <typename Key, typename Value>
    class HashLruCaches
    {
    public:
        /**
         * @brief 构造函数：初始化分片LRU的总容量、分片数及各分片LRU实例
         * @param capacity 缓存总容量（所有分片的容量之和）
         * @param sliceNum 分片数量（建议设为CPU核心数，最大化并行性）
         * @初始化逻辑：
         *  1. 分片数校验：若传入≤0，则自动使用硬件CPU核心数（std::thread::hardware_concurrency()）；
         *  2. 分片容量计算：总容量 / 分片数，向上取整（避免容量浪费）；
         *  3. 初始化每个分片：创建独立的LruCache实例，用unique_ptr管理生命周期。
         */
        HashLruCaches(size_t capacity, int sliceNum)
            : capacity_(capacity), // 缓存总容量
                                   // 分片数：优先用传入值，无效则用CPU核心数（合理的默认值）
              sliceNum_(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency())
        {
            // 计算每个分片的容量：向上取整（例如总容量10，分片数3 → 每个分片容量4）
            size_t sliceSize = std::ceil(capacity / static_cast<double>(sliceNum_));

            // 初始化所有分片：每个分片对应一个独立的LruCache实例
            for (int i = 0; i < sliceNum_; ++i)
            {
                // emplace_back：直接构造LruCache并封装为unique_ptr，避免拷贝
                lruSliceCaches_.emplace_back(new LruCache<Key, Value>(sliceSize));
            }
        }

        /**
         * @brief 添加/更新缓存项（线程安全，分片级锁）
         * @param key 缓存键
         * @param value 缓存值
         * @核心流程：
         *  1. 对key做哈希运算，取模得到分片索引；
         *  2. 调用对应分片的put方法（仅抢占该分片的锁，不影响其他分片）；
         *  3. 不同分片的put操作可并行执行，提升高并发性能。
         */
        void put(Key key, Value value)
        {
            // 步骤1：计算key所属的分片索引（哈希取模保证均匀分布）
            size_t sliceIndex = Hash(key) % sliceNum_;
            // 步骤2：调用对应分片的put方法
            lruSliceCaches_[sliceIndex]->put(key, value);
        }

        /**
         * @brief 获取缓存项（线程安全，分片级锁，推荐使用）
         * @param key 缓存键
         * @param value 输出参数：找到则赋值，未找到保持原值
         * @return bool 找到返回true，未找到返回false
         * @核心流程：与put一致，先路由到分片，再调用分片的get方法
         * @优势：通过引用返回值，避免复杂类型（如std::string）的默认构造/拷贝开销
         */
        bool get(Key key, Value &value)
        {
            size_t sliceIndex = Hash(key) % sliceNum_;
            return lruSliceCaches_[sliceIndex]->get(key, value);
        }

        /**
         * @brief 重载get方法：直接返回缓存值（不推荐用于复杂类型）
         * @param key 缓存键
         * @return Value 找到返回对应值，未找到返回默认初始化的值
         * @风险提示：
         *  1. memset(&value, 0, sizeof(value)) 仅适用于POD类型（如int/float/结构体）；
         *  2. 对非POD类型（如std::string、std::vector）使用memset会破坏对象内部结构，导致崩溃；
         *  3. 建议优先使用带引用参数的get方法。
         */
        Value get(Key key)
        {
            Value value;
            // 潜在风险：memset按字节清零，复杂类型禁用！
            memset(&value, 0, sizeof(value));
            get(key, value);
            return value;
        }

    private:
        /**
         * @brief 私有哈希函数：将key映射为size_t类型的哈希值
         * @param key 缓存键
         * @return size_t key的哈希值（基于std::hash实现，保证均匀分布）
         * @设计说明：
         *  1. 使用C++标准库std::hash，适配大部分基础类型（int/string等）；
         *  2. 若Key为自定义类型，需特化std::hash或重写该Hash函数，保证哈希均匀性。
         */
        size_t Hash(Key key)
        {
            std::hash<Key> hashFunc; // 标准哈希函数对象
            return hashFunc(key);    // 计算key的哈希值
        }

    private:
        size_t capacity_; ///< 缓存总容量（所有分片的容量之和）
        int sliceNum_;    ///< 分片数量（核心配置，决定并发性能）
        /**
         * @brief 分片LRU容器：存储所有分片的LruCache实例
         * @细节：
         *  1. unique_ptr：独占所有权，确保分片LRU的生命周期与HashLruCaches一致，自动释放；
         *  2. vector：随机访问O(1)，快速根据分片索引找到对应LRU实例；
         *  3. 每个分片的LruCache自带互斥锁，实现分片级线程安全。
         */
        std::vector<std::unique_ptr<LruCache<Key, Value>>> lruSliceCaches_;
    };

}
