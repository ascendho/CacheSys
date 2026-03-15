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
        using LruNodeType = LruNode<Key, Value>;          // LRU节点类型
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
         * @note 修正原代码：dummyTail_prev_ → dummyTail_->prev_（笔误）
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
}