#pragma once

#include <atomic>
#include <cstddef>

namespace CacheSys
{
    /**
     * @brief 缓存运行状态统计结构体
     * @details 记录缓存的核心运行指标，用于监控缓存性能、计算命中率等
     * 核心指标说明：
     * - hits:      缓存命中次数（请求的key存在于缓存中）
     * - misses:    缓存未命中次数（请求的key不存在于缓存中）
     * - evictions: 缓存淘汰次数（缓存满时移除旧数据的次数）
     */
    struct CacheStats
    {
        size_t hits = 0;
        size_t misses = 0;
        size_t evictions = 0;

        /**
         * @brief 计算并返回缓存命中率
         * @return 命中率（范围 [0, 1]），无请求时返回0.0
         * @details 命中率 = 命中次数 / (命中次数 + 未命中次数)，是评估缓存有效性的核心指标
         */
        double hitRate() const
        {
            size_t total = hits + misses;
            // 避免除零错误：无请求时返回0.0，否则计算命中率
            return total ? static_cast<double>(hits) / total : 0.0;
        }

        size_t totalRequests() const { return hits + misses; }
    };

    template <typename Key, typename Value>
    class CachePolicy
    {
    public:
        /**
         * @brief 虚析构函数（默认实现）
         * @details 抽象基类必须声明虚析构函数，确保子类对象通过基类指针销毁时，
         *          能正确调用子类的析构函数，避免内存泄漏
         */
        virtual ~CachePolicy() = default;

        /**
         * @brief 写入/更新缓存项（纯虚函数，子类必须实现）
         * @details 若key已存在则更新值，若不存在则新增；缓存满时触发淘汰策略
         */
        virtual void put(Key key, Value value) = 0;

        /**
         * @brief 读取缓存项（纯虚函数，子类必须实现）
         * @return bool - true：命中（value被赋值），false：未命中（value无意义）
         * @details 命中时自动更新统计（hits++），未命中时更新misses++
         */
        virtual bool get(Key key, Value &value) = 0;

        /**
         * @brief 便捷读取接口（纯虚函数，子类必须实现）
         * @return Value - 命中时返回对应值，未命中时返回Value的默认构造值
         * @details 封装get(Key, Value&)，简化无输出参数的读取场景，统计规则与get一致
         */
        virtual Value get(Key key) = 0;

        /**
         * @brief 移除指定缓存项（虚函数，默认空实现）
         * @details 可选接口：子类可根据自身实现逻辑覆盖（如需要清理链表/哈希表节点），
         *          未覆盖时调用无副作用
         */
        virtual void remove(Key key) {}

        /**
         * @brief 获取当前缓存统计快照
         * @return CacheStats - 包含hits/misses/evictions的只读统计数据
         * @details 读取原子变量的当前值，保证并发场景下统计数据的一致性，
         *          返回的是拷贝，不会影响原统计值
         */
        CacheStats getStats() const
        {
            // 原子变量的load()操作保证线程安全，获取当前计数并构造统计对象
            return {hits_.load(), misses_.load(), evictions_.load()};
        }

        /**
         * @brief 重置所有统计值为0
         * @details 原子变量直接赋值，支持并发场景下的重置操作，
         *          用于清空历史统计、重新开始计数
         */
        void resetStats()
        {
            hits_ = 0;
            misses_ = 0;
            evictions_ = 0;
        }

    protected:
        /**
         * @brief 缓存命中次数（原子变量）
         * @note mutable：允许const成员函数修改（如get()命中时更新）；
         *       atomic：保证多线程并发读写时的线程安全，无需额外加锁
         */
        mutable std::atomic<size_t> hits_{0};
        /**
         * @brief 缓存未命中次数（原子变量）
         * @note 设计同hits_，用于统计缓存未命中的次数
         */
        mutable std::atomic<size_t> misses_{0};
        /**
         * @brief 缓存淘汰次数（原子变量）
         * @note 设计同hits_，用于统计缓存满时淘汰旧数据的次数
         */
        mutable std::atomic<size_t> evictions_{0};
    };
}