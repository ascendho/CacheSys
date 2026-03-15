#pragma once

#include <cmath>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <memory>

#include "CachePolicy.h"

namespace CacheSys
{
    // 前向声明：LfuCache需作为FreqList的友元类，提前声明避免编译依赖
    template <typename Key, typename Value>
    class LfuCache;

    /**
     * @class FreqList
     * @brief LFU缓存的「频率桶」核心类：存储相同访问频率的所有节点，基于双向链表实现
     * @tparam Key 缓存键类型（需支持相等比较）
     * @tparam Value 缓存值类型
     * @核心设计：
     *  1. 每个FreqList实例对应一个固定的访问频率（freq_），存储所有该频率的缓存节点；
     *  2. 双向链表+虚拟头尾节点：简化节点的插入/删除操作，避免边界判断；
     *  3. 智能指针（shared_ptr+weak_ptr）：管理节点生命周期，避免循环引用导致的内存泄漏；
     *  4. 节点插入到链表尾部（标记为「最近访问」），淘汰时取链表头部（最久未访问）。
     */
    template <typename Key, typename Value>
    class FreqList
    {
    private:
        /**
         * @struct Node
         * @brief 频率桶内的节点结构，存储缓存键值、访问频率及链表指针
         * @note 作为FreqList的私有内部结构，仅供频率桶自身管理
         */
        struct Node
        {
            int freq;                   // 该节点的访问频率（初始值为1）
            Key key;                    // 缓存键
            Value value;                // 缓存值
            std::weak_ptr<Node> pre;    // 前驱节点（weak_ptr：打破双向链表的循环引用）
            std::shared_ptr<Node> next; // 后继节点（shared_ptr：持有所有权，管理节点生命周期）

            /**
             * @brief 空构造函数：初始化频率为1，后继指针为空（用于创建虚拟头尾节点）
             */
            Node() : freq(1), next(nullptr) {}

            /**
             * @brief 带参构造函数：初始化键值对，频率默认1，后继指针为空（用于创建真实缓存节点）
             * @param key 缓存键
             * @param value 缓存值
             */
            Node(Key key, Value value) : freq(1), key(key), value(value), next(nullptr) {}
        };

        using NodePtr = std::shared_ptr<Node>; // 节点智能指针别名，简化代码书写
        int freq_;                             // 当前频率桶对应的访问频率（如freq_=3表示存储访问次数为3的节点）
        NodePtr head_;                         // 双向链表虚拟头节点（简化头部操作）
        NodePtr tail_;                         // 双向链表虚拟尾节点（简化尾部操作）

    public:
        /**
         * @brief 构造函数：初始化频率桶的频率值和双向链表（虚拟头尾节点）
         * @param n 当前频率桶对应的访问频率（如n=2则该桶存储访问次数为2的节点）
         * @初始化逻辑：
         *  1. freq_ = n：绑定当前桶的频率值；
         *  2. 创建虚拟头/尾节点，初始化链表：head_->next → tail_，tail_->pre → head_；
         *  3. 虚拟节点无实际键值，仅作为链表边界，避免空指针判断。
         */
        explicit FreqList(int n) : freq_(n)
        {
            head_ = std::make_shared<Node>(); // 创建虚拟头节点（空节点）
            tail_ = std::make_shared<Node>(); // 创建虚拟尾节点（空节点）
            head_->next = tail_;              // 头节点后继指向尾节点
            tail_->pre = head_;               // 尾节点前驱指向头节点
        }

        /**
         * @brief 判断当前频率桶是否为空（无有效缓存节点）
         * @return bool 空返回true，非空返回false
         * @判断逻辑：虚拟头节点的后继直接指向虚拟尾节点 → 无有效节点
         */
        bool isEmpty() const
        {
            return head_->next == tail_;
        }

        /**
         * @brief 将节点添加到频率桶链表的尾部（标记为「最近访问」）
         * @param node 要添加的节点指针
         * @核心逻辑：
         *  1. 先做空指针校验，避免非法访问；
         *  2. 新节点的前驱指向尾节点的原前驱，后继指向尾节点；
         *  3. 调整尾节点原前驱的后继、尾节点的前驱，完成插入；
         *  4. 尾部插入表示该节点是「同频率下最近访问」的，淘汰时优先淘汰头部节点。
         */
        void addNode(NodePtr node)
        {
            // 空指针校验：节点/头/尾节点为空则直接返回
            if (!node || !head_ || !tail_)
            {
                return;
            }

            node->pre = tail_->pre;         // 新节点前驱 = 尾节点的原前驱
            node->next = tail_;             // 新节点后继 = 尾节点
            tail_->pre.lock()->next = node; // 尾节点原前驱的后继 = 新节点（weak_ptr转shared_ptr）
            tail_->pre = node;              // 尾节点的前驱 = 新节点
        }

        /**
         * @brief 从频率桶链表中移除指定节点
         * @param node 要移除的节点指针
         * @核心逻辑：
         *  1. 多层空指针/有效性校验，避免非法操作；
         *  2. weak_ptr::lock()获取前驱节点的shared_ptr（确保前驱未被释放）；
         *  3. 调整前驱、后继节点的指针，断开当前节点与链表的连接；
         *  4. 清空节点的后继指针，彻底断开引用。
         */
        void removeNode(NodePtr node)
        {
            // 空指针校验
            if (!node || !head_ || !tail_)
            {
                return;
            }
            // 前驱节点过期（已释放）或后继为空 → 节点无效，直接返回
            // 前驱节点的所有 shared_ptr 都被释放，前驱已销毁
            // 正常链表中节点的 next 至少指向 tail_，为空说明节点已脱离链表
            if (node->pre.expired() || !node->next)
                return;

            auto pre = node->pre.lock(); // weak_ptr转shared_ptr，获取前驱节点
            pre->next = node->next;      // 前驱节点的后继 = 当前节点的后继
            node->next->pre = pre;       // 当前节点的后继的前驱 = 前驱节点
            node->next = nullptr;        // 清空当前节点的后继，彻底断开连接
        }

        /**
         * @brief 获取频率桶链表中的第一个有效节点（最久未访问）
         * @return NodePtr 第一个有效节点的指针（虚拟头节点的后继）
         * @工程意义：LFU淘汰时，先找最小频率桶，再淘汰该桶的第一个节点（同频率下最久未访问）
         */
        NodePtr getFirstNode() const { return head_->next; }

        // 友元声明：LfuCache需直接访问频率桶的私有成员（如freq_、head_、tail_），简化逻辑
        friend class LfuCache<Key, Value>;
    };

    /**
     * @class LfuCache
     * @brief 带频率衰减优化的LFU（最不经常使用）缓存实现类，继承统一缓存策略接口
     * @tparam Key 缓存键类型（需支持std::hash哈希运算和相等比较）
     * @tparam Value 缓存值类型
     * @extends CachePolicy<Key, Value> 遵循统一缓存策略接口，保证策略可替换性
     * @核心优化点：
     *  1. 频率衰减机制（maxAverageNum_/curAverageNum_）：解决基础LFU的频率爆炸、过时热点占用问题；
     *  2. 线程安全（std::mutex）：全局锁保证多线程下数据一致性；
     *  3. 最小频率追踪（minFreq_）：快速定位要淘汰的节点，避免遍历所有频率；
     *  4. 模块化拆分（Internal方法）：将核心逻辑拆分，提升代码复用性和可维护性。
     */
    template <typename Key, typename Value>
    class LfuCache : public CachePolicy<Key, Value>
    {
    public:
        // 类型别名：简化代码书写，提升可读性
        using Node = typename FreqList<Key, Value>::Node; // 频率桶节点类型（复用FreqList的Node结构）
        using NodePtr = std::shared_ptr<Node>;            // 节点智能指针别名
        using NodeMap = std::unordered_map<Key, NodePtr>; // 键→节点的哈希表别名

        /**
         * @brief 构造函数：初始化LFU缓存的核心参数
         * @param capacity 缓存最大容量（超过则触发淘汰）
         * @param maxAverageNum 频率平均阈值（默认100万）：用于频率衰减，解决频率爆炸问题
         * @初始化逻辑：
         *  1. capacity_：缓存总容量；
         *  2. minFreq_：初始化为INT8_MAX（最小频率，后续动态更新）；
         *  3. maxAverageNum_：频率衰减的阈值，超过则触发频率平均/衰减；
         *  4. curAverageNum_/curTotalNum_：频率衰减的计数变量，初始化为0；
         *  5. 其他容器（nodeMap_/freqToFreqList_）默认空初始化。
         */
        LfuCache(int capacity, int maxAverageNum = 1000000)
            : capacity_(capacity), minFreq_(INT8_MAX),
              maxAverageNum_(maxAverageNum), curAverageNum_(0), curTotalNum_(0) {}

        /**
         * @brief 析构函数：默认实现
         * @note 智能指针（NodePtr）自动管理节点内存，哈希表自动析构，无需手动释放资源；
         *       虚析构保证基类指针（CachePolicy*）能正确析构派生类对象。
         */
        ~LfuCache() override = default;

        /**
         * @brief 添加/更新缓存项（线程安全，重载父类接口）
         * @param key 缓存键
         * @param value 缓存值
         * @核心逻辑：
         *  1. 容量为0时直接返回（无效缓存）；
         *  2. 加锁保证线程安全（std::lock_guard基于RAII，自动释放锁）；
         *  3. 键已存在：更新值 + 调用getInternal（更新频率）；
         *  4. 键不存在：调用putInternal（新增节点，容量满则淘汰）。
         */
        void put(Key key, Value value) override
        {
            if (capacity_ == 0)
                return;

            std::lock_guard<std::mutex> lock(mutex_); // 加锁：作用域结束自动释放，防止死锁
            auto it = nodeMap_.find(key);
            if (it != nodeMap_.end())
            {
                it->second->value = value;      // 更新已存在节点的值
                getInternal(it->second, value); // 更新节点频率（复用getInternal逻辑）
                return;
            }

            putInternal(key, value); // 新增节点的核心逻辑
        }

        /**
         * @brief 获取缓存项（线程安全，重载父类接口，推荐使用）
         * @param key 缓存键
         * @param value 输出参数：找到则赋值，未找到保持原值
         * @return bool 找到返回true，未找到返回false
         * @核心逻辑：
         *  1. 加锁保证线程安全；
         *  2. 查找键是否存在：存在则调用getInternal（更新频率）并返回true；
         *  3. 不存在则返回false。
         */
        bool get(Key key, Value &value) override
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = nodeMap_.find(key);
            if (it != nodeMap_.end())
            {
                getInternal(it->second, value); // 更新节点频率并赋值输出参数
                return true;
            }

            return false;
        }

        /**
         * @brief 重载get方法：直接返回缓存值（线程安全）
         * @param key 缓存键
         * @return Value 找到返回对应值，未找到返回Value的默认构造值
         * @适用场景：简单类型（如int/float），复杂类型建议使用带引用的get方法（避免拷贝开销）
         * @逻辑复用：基于带引用的get方法实现，保证逻辑一致性。
         */
        Value get(Key key) override
        {
            Value value;
            get(key, value); // 复用带引用的get逻辑
            return value;
        }

        /**
         * @brief 清空缓存（线程安全）
         * @核心逻辑：清空「键→节点」映射和「频率→频率桶」映射，释放所有节点资源；
         * @使用场景：缓存失效、业务重置时快速清空所有数据。
         */
        void purge()
        {
            nodeMap_.clear();        // 清空键到节点的映射
            freqToFreqList_.clear(); // 清空频率到频率桶的映射
        }

    private:
        /**
         * @brief 私有内部方法：新增节点的核心逻辑（仅在put中调用，已加锁）
         * @param key 缓存键
         * @param value 缓存值
         * @核心逻辑：创建新节点→添加到频率桶→容量满则淘汰节点→更新频率计数/最小频率。
         */
        void putInternal(Key key, Value value);

        /**
         * @brief 私有内部方法：更新节点频率的核心逻辑（仅在get/put中调用，已加锁）
         * @param node 要更新的节点指针
         * @param value 输出参数：赋值节点的value（供get方法返回）
         * @核心逻辑：移除节点旧频率桶→频率+1→添加到新频率桶→更新最小频率→处理频率衰减。
         */
        void getInternal(NodePtr node, Value &value);

        /**
         * @brief 私有内部方法：淘汰节点（缓存容量满时调用，已加锁）
         * @核心逻辑：找到minFreq_对应的频率桶→淘汰桶内第一个节点（同频率最久未访问）→
         *           从nodeMap_/freqToFreqList_中移除→更新计数/最小频率。
         */
        void kickOut();

        /**
         * @brief 私有内部方法：将节点从对应频率桶中移除（已加锁）
         * @param node 要移除的节点指针
         * @逻辑复用：getInternal（频率更新）、kickOut（淘汰节点）时都会调用。
         */
        void removeFromFreqList(NodePtr node);

        /**
         * @brief 私有内部方法：将节点添加到对应频率桶的尾部（已加锁）
         * @param node 要添加的节点指针
         * @逻辑复用：putInternal（新增节点）、getInternal（频率更新）时都会调用。
         */
        void addToFreqList(NodePtr node);

        /**
         * @brief 私有内部方法：增加频率计数（频率衰减用，已加锁）
         * @核心逻辑：更新curAverageNum_/curTotalNum_，用于判断是否触发频率衰减。
         */
        void addFreqNum();

        /**
         * @brief 私有内部方法：减少频率计数（频率衰减用，已加锁）
         * @param num 要减少的计数值
         * @核心逻辑：淘汰节点/频率衰减时，更新curAverageNum_/curTotalNum_。
         */
        void decreaseFreqNum(int num);

        /**
         * @brief 私有内部方法：处理频率超过最大平均阈值的情况（频率衰减核心，已加锁）
         * @核心逻辑：当curAverageNum_ ≥ maxAverageNum_时，触发频率衰减（如所有节点频率/2），
         *           解决基础LFU的频率爆炸、过时热点占用问题。
         */
        void HandleOverMaxAverageNum();

        /**
         * @brief 私有内部方法：更新当前最小频率minFreq_（已加锁）
         * @核心逻辑：节点频率更新/淘汰后，重新计算最小频率，保证kickOut能快速定位淘汰桶。
         */
        void updateMinFreq();

    private:
        int capacity_;      ///< 缓存最大容量：超过则触发kickOut淘汰节点
        int minFreq_;       ///< 当前缓存中最小访问频率：快速定位要淘汰的频率桶，避免遍历
        int maxAverageNum_; ///< 频率平均阈值：超过则触发HandleOverMaxAverageNum（频率衰减）
        int curAverageNum_; ///< 当前频率累计值：用于频率衰减的计数
        int curTotalNum_;   ///< 当前缓存节点总数：辅助频率衰减计算
        std::mutex mutex_;  ///< 全局互斥锁：保证所有操作的线程安全，防止多线程竞争
        NodeMap nodeMap_;   ///< 键→节点的哈希表：O(1)查找节点，核心映射关系
        /**
         * @brief 频率→频率桶的哈希表：O(1)找到对应频率的FreqList实例
         * @note 存储指针而非对象：减少拷贝开销，统一管理频率桶的生命周期
         */
        std::unordered_map<int, FreqList<Key, Value> *> freqToFreqList_;
    };

    // 模板参数说明：Key - 缓存键的类型，Value - 缓存值的类型
    template <typename Key, typename Value>
    // 缓存命中时的内部处理函数（核心逻辑：更新节点访问频率，维护频率列表和最小频率）
    // 参数：node - 命中的缓存节点，value - 输出参数，用于返回节点的缓存值
    void LfuCache<Key, Value>::getInternal(NodePtr node, Value &value)
    {
        // 1. 将命中节点的value赋值给输出参数，返回给调用方
        value = node->value;

        // 2. 先将节点从当前所属的频率列表中移除（因为频率要更新）
        removeFromFreqList(node);

        // 3. 节点的访问频率+1（命中一次，频率递增）
        node->freq++;

        // 4. 将节点添加到新的频率列表中（频率+1后的列表）
        addToFreqList(node);

        // 5. 维护最小频率minFreq_：
        //    如果当前节点更新前的频率（node->freq - 1）等于最小频率，且原频率列表已空
        //    说明原最小频率对应的节点都被移走了，需要将最小频率+1
        if (node->freq - 1 == minFreq_ && !freqToFreqList_[minFreq_]->isEmpty())
        {
            minFreq_++;
        }

        // 6. 更新总访问频次和平均访问频次（用于后续频率衰减判断）
        addFreqNum();
    }

    template <typename Key, typename Value>
    // 缓存插入的内部处理函数（核心逻辑：容量检查→淘汰→创建节点→加入频率列表）
    // 参数：key - 要插入的缓存键，value - 要插入的缓存值
    void LfuCache<Key, Value>::putInternal(Key key, Value value)
    {
        // 1. 检查缓存是否已满（达到设定的容量上限capacity_）
        if (nodeMap_.size() == capacity_)
        {
            // 2. 缓存满时，执行淘汰逻辑（淘汰最不经常使用的节点）
            kickOut();
        }

        // 3. 创建新的缓存节点（使用智能指针管理，避免内存泄漏）
        NodePtr node = std::make_shared<Node>(key, value);

        // 4. 将新节点加入键-节点映射表（方便快速查找）
        nodeMap_[key] = node;

        // 5. 将新节点添加到频率为1的列表中（新节点初始访问频率为1）
        addToFreqList(node);

        // 6. 更新总访问频次和平均访问频次
        addFreqNum();

        // 7. 维护最小频率：新节点频率为1，因此最小频率至少是1
        minFreq_ = std::min(minFreq_, 1);
    }

    template <typename Key, typename Value>
    // 缓存淘汰函数（核心逻辑：淘汰最小频率列表中的第一个节点，LFU核心淘汰规则）
    void LfuCache<Key, Value>::kickOut()
    {
        // 1. 获取最小频率（minFreq_）对应的频率列表中的第一个节点（最久未使用的节点）
        //    注：LFU通常结合LRU，频率相同的节点淘汰最久未使用的
        NodePtr node = freqToFreqList_[minFreq_]->getFirstNode();

        // 2. 将该节点从其所属的频率列表中移除
        removeFromFreqList(node);

        // 3. 从键-节点映射表中删除该节点（缓存中移除）
        nodeMap_.erase(node->key);

        // 4. 减少总访问频次（淘汰节点，其频率从总频次中扣除）
        decreaseFreqNum(node->freq);
    }

    template <typename Key, typename Value>
    // 将节点从对应频率的列表中移除
    // 参数：node - 要移除的缓存节点
    void LfuCache<Key, Value>::removeFromFreqList(NodePtr node)
    {
        // 空节点直接返回，避免空指针操作
        if (!node)
            return;

        // 获取节点当前的访问频率
        auto freq = node->freq;

        // 从该频率对应的列表中移除节点
        freqToFreqList_[freq]->removeNode(node);
    }

    template <typename Key, typename Value>
    // 将节点添加到对应频率的列表中（频率列表不存在则创建）
    // 参数：node - 要添加的缓存节点
    void LfuCache<Key, Value>::addToFreqList(NodePtr node)
    {
        // 空节点直接返回
        if (!node)
            return;

        // 获取节点当前的访问频率
        auto freq = node->freq;

        // 检查该频率对应的列表是否存在，不存在则新建
        if (freqToFreqList_.find(freq) == freqToFreqList_.end())
        {
            freqToFreqList_[freq] = new FreqList<Key, Value>(freq);
        }

        // 将节点添加到该频率对应的列表中
        freqToFreqList_[freq]->addNode(node);
    }

    template <typename Key, typename Value>
    // 更新总访问频次和平均访问频次（核心：用于判断是否需要触发频率衰减）
    void LfuCache<Key, Value>::addFreqNum()
    {
        // 总访问频次+1（每次命中/插入都会调用，频次累加）
        curTotalNum_++;

        // 计算当前平均访问频次：总频次 / 缓存节点数量
        if (nodeMap_.empty())
            curAverageNum_ = 0; // 缓存为空时，平均频次为0
        else
            curAverageNum_ = curTotalNum_ / nodeMap_.size();

        // 如果平均访问频次超过设定的最大值，触发频率衰减逻辑
        if (curAverageNum_ > maxAverageNum_)
        {
            HandleOverMaxAverageNum();
        }
    }

    template <typename Key, typename Value>
    // 减少总访问频次并更新平均访问频次（淘汰节点时调用）
    // 参数：num - 要扣除的频次（即被淘汰节点的访问频率）
    void LfuCache<Key, Value>::decreaseFreqNum(int num)
    {
        // 总访问频次扣除被淘汰节点的频率
        curTotalNum_ -= num;

        // 重新计算平均访问频次
        if (nodeMap_.empty())
            curAverageNum_ = 0;
        else
            curAverageNum_ = curTotalNum_ / nodeMap_.size();
    }

    template <typename Key, typename Value>
    // 频率衰减处理函数（核心：当平均访问频次过高时，降低所有节点的访问频率）
    // 目的：防止某些节点长期高频次占用缓存，导致新节点无法被保留
    void LfuCache<Key, Value>::HandleOverMaxAverageNum()
    {
        // 缓存为空时直接返回
        if (nodeMap_.empty())
            return;

        // 遍历所有缓存节点，统一衰减访问频率
        for (auto it = nodeMap_.begin(); it != nodeMap_.end(); ++it)
        {
            // 跳过空节点，避免空指针操作
            if (!it->second)
                continue;

            NodePtr node = it->second;

            // 1. 先将节点从当前频率列表中移除（频率要修改）
            removeFromFreqList(node);

            // 2. 记录节点衰减前的频率
            int oldFreq = node->freq;

            // 3. 计算衰减值：最大平均频次的一半（可调整的衰减策略）
            int decay = maxAverageNum_ / 2;

            // 4. 节点频率衰减
            node->freq -= decay;

            // 5. 频率不能低于1（保证节点至少有基础访问频率）
            if (node->freq < 1)
                node->freq = 1;

            // 6. 更新总访问频次：总频次 += （新频率 - 旧频率）
            int delta = node->freq - oldFreq;
            curTotalNum_ += delta;

            // 7. 将节点添加到衰减后的新频率列表中
            addToFreqList(node);
        }

        // 8. 衰减后重新计算最小频率（因为所有节点频率都可能变化）
        updateMinFreq();
    }

    template <typename Key, typename Value>
    // 更新缓存的最小访问频率（遍历所有非空频率列表，找到最小值）
    void LfuCache<Key, Value>::updateMinFreq()
    {
        // 初始化最小频率为char类型的最大值（INT8_MAX）
        minFreq_ = INT8_MAX;

        // 遍历所有频率→频率列表的映射
        for (const auto &pair : freqToFreqList_)
        {
            // 仅考虑非空的频率列表
            if (pair.second && !pair.second->isEmpty())
            {
                // 更新最小频率为当前最小值和该频率的较小值
                minFreq_ = std::min(minFreq_, pair.first);
            }
        }

        // 如果所有频率列表都为空（缓存空），重置最小频率为1
        if (minFreq_ == INT8_MAX)
            minFreq_ = 1;
    }

    // 核心设计说明：该实现并未额外占用内存（牺牲空间），而是将原有缓存总容量拆分为多个独立分片，
    // 每个分片独享一部分容量，核心目的是降低锁粒度、提升高并发下的性能
    template <typename Key, typename Value>
    class HashLfuCache
    {
    public:
        /**
         * @brief 构造函数：初始化分片LFU缓存的总容量、分片数及各分片LFU实例
         * @param capacity 缓存总容量（所有分片的容量之和，无额外空间开销）
         * @param sliceNum 分片数量（建议设为CPU核心数，最大化并行性）
         * @param maxAverageNum 频率衰减阈值（透传给每个KLfuCache实例，解决LFU频率爆炸问题）
         * @核心逻辑：
         *  1. 分片数校验：传入≤0时，自动使用硬件CPU核心数（std::thread::hardware_concurrency()）；
         *  2. 分片容量计算：总容量 / 分片数，向上取整（避免容量浪费，如总容量10、分片数3→每个分片4）；
         *  3. 初始化分片：为每个分片创建独立的KLfuCache实例，用unique_ptr管理生命周期（自动释放）。
         */
        HashLfuCache(size_t capacity, int sliceNum, int maxAverageNum = 10)
            : sliceNum_(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency()) // 分片数：优先传参，无效则用CPU核心数
              ,
              capacity_(capacity) // 缓存总容量（拆分到各分片，无额外空间）
        {
            // 计算每个LFU分片的容量：向上取整，保证总容量不小于设定值
            size_t sliceSize = std::ceil(capacity_ / static_cast<double>(sliceNum_));
            // 初始化所有分片：每个分片对应一个独立的KLfuCache实例
            for (int i = 0; i < sliceNum_; ++i)
            {
                lfuSliceCaches_.emplace_back(new KLfuCache<Key, Value>(sliceSize, maxAverageNum));
            }
        }

        /**
         * @brief 添加/更新缓存项（线程安全，分片级锁隔离）
         * @param key 缓存键
         * @param value 缓存值
         * @核心流程：
         *  1. 对key做哈希运算，取模得到分片索引（保证key均匀分布到不同分片）；
         *  2. 调用对应分片的put方法（仅抢占该分片的锁，不影响其他分片）；
         *  3. 不同分片的put操作可并行执行，解决基础LFU全局锁的并发瓶颈。
         */
        void put(Key key, Value value)
        {
            // 哈希路由：计算key所属的分片索引（均匀分布核心逻辑）
            size_t sliceIndex = Hash(key) % sliceNum_;
            // 调用对应分片的put方法
            lfuSliceCaches_[sliceIndex]->put(key, value);
        }

        /**
         * @brief 获取缓存项（线程安全，分片级锁隔离，推荐使用）
         * @param key 缓存键
         * @param value 输出参数：找到则赋值，未找到保持原值
         * @return bool 找到返回true，未找到返回false
         * @核心流程：与put一致，先路由到分片，再调用分片的get方法
         * @优势：通过引用返回值，避免复杂类型（如std::string）的默认构造/拷贝开销
         */
        bool get(Key key, Value &value)
        {
            // 哈希路由：找到key对应的LFU分片
            size_t sliceIndex = Hash(key) % sliceNum_;
            // 调用对应分片的get方法，返回查找结果
            return lfuSliceCaches_[sliceIndex]->get(key, value);
        }

        /**
         * @brief 重载get方法：直接返回缓存值（线程安全）
         * @param key 缓存键
         * @return Value 找到返回对应值，未找到返回Value的默认构造值
         * @适用场景：简单类型（如int/float），复杂类型建议使用带引用的get方法
         * @逻辑复用：基于带引用的get方法实现，保证逻辑一致性
         */
        Value get(Key key)
        {
            Value value;
            get(key, value); // 复用带引用的get逻辑
            return value;
        }

        /**
         * @brief 清空所有缓存（线程安全）
         * @核心逻辑：遍历所有LFU分片，调用每个分片的purge方法，清空分片内的所有数据；
         * @使用场景：缓存失效、业务重置时快速清空整个分片LFU的所有数据。
         */
        void purge()
        {
            for (auto &lfuSliceCache : lfuSliceCaches_)
            {
                lfuSliceCache->purge(); // 调用分片的清空方法
            }
        }

    private:
        /**
         * @brief 私有哈希函数：将key映射为size_t类型的哈希值（路由核心）
         * @param key 缓存键
         * @return size_t key的哈希值（基于std::hash实现，保证均匀分布）
         * @注意事项：
         *  1. 若Key为自定义类型，需特化std::hash或重写该Hash函数，保证哈希均匀性；
         *  2. 均匀哈希是分片负载均衡的关键，避免单分片过载。
         */
        size_t Hash(Key key)
        {
            std::hash<Key> hashFunc; // C++标准哈希函数对象（适配基础类型：int/string等）
            return hashFunc(key);    // 计算key的哈希值
        }

    private:
        size_t capacity_; ///< 缓存总容量：拆分到所有分片，无额外空间开销（核心设计点）
        int sliceNum_;    ///< 缓存分片数量：决定并发粒度，建议等于CPU核心数
        /**
         * @brief LFU分片容器：存储所有独立的KLfuCache实例
         * @细节：
         *  1. unique_ptr：独占所有权，确保分片实例的生命周期与HashLfuCache一致，自动释放；
         *  2. vector：随机访问O(1)，快速根据分片索引找到对应KLfuCache实例；
         *  3. 每个KLfuCache实例自带互斥锁，实现分片级线程安全。
         */
        std::vector<std::unique_ptr<KLfuCache<Key, Value>>> lfuSliceCaches_;
    };
}