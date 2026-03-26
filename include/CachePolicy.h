#pragma once

#include <atomic>
#include <cstddef>

/**
 * @namespace CacheSys
 * @brief 缓存系统的核心命名空间，包含统计数据结构和缓存策略基类
 */
namespace CacheSys
{
    /**
     * @struct CacheStats
     * @brief 用于记录缓存运行指标的统计结构体
     */
    struct CacheStats
    {
        size_t hits = 0;      ///< 命中次数：在缓存中成功找到数据的次数
        size_t misses = 0;    ///< 未命中次数：请求的数据不在缓存中的次数
        size_t evictions = 0; ///< 驱逐次数：由于缓存满而移除旧数据的次数

        /**
         * @brief 获取总请求次数（命中 + 未命中）
         * @return 总请求次数
         */
        size_t totalRequests() const { return hits + misses; }

         /**
         * @brief 计算当前的缓存命中率
         * @return 命中率 (0.0 到 1.0)，如果总请求为0则返回 0.0
         */
        double hitRate() const
        {
            size_t total = totalRequests();
            return total ? static_cast<double>(hits) / total : 0.0;
        }
    };

    /**
     * @brief 缓存策略抽象基类
     * @tparam Key 缓存键的类型
     * @tparam Value 缓存值的类型
     * 
     * 该类定义了所有缓存实现（如 LRU, LFU, FIFO）必须遵循的标准接口。
     * 使用了原子变量 (std::atomic) 以支持多线程环境下的统计信息更新。
     */
    template <typename Key, typename Value>
    class CachePolicy
    {
    public:
        /**
         * @brief 虚析构函数，确保派生类对象能被正确销毁
         */
        virtual ~CachePolicy() = default;

        /**
         * @brief 将数据放入缓存
         * @param key 键
         * @param value 值
         * @note 子类实现应处理空间不足时的驱逐逻辑，并增加 evictions_ 计数
         */
        virtual void put(Key key, Value value) = 0;

        /**
         * @brief 从缓存中获取数据（引用方式）
         * @param key 要查找的键
         * @param value [out] 用于存储找到的值
         * @return bool 如果命中返回 true，否则返回 false
         * @note 内部应根据结果更新 hits_ 或 misses_ 计数
         */
        virtual bool get(Key key, Value &value) = 0;

        /**
         * @brief 从缓存中获取数据（直接返回方式）
         * @param key 要查找的键
         * @return Value 返回找到的值，若未命中通常返回该类型的默认构造值
         */
        virtual Value get(Key key) = 0;

        /**
         * @brief 从缓存中显式移除指定的键
         * @param key 要移除的键
         * @note 默认空实现：remove 属于可选能力，避免强制所有派生类都提供删除语义；
         *       需要显式删除并维护内部结构一致性的策略（如 LRU/LFU/ARC/LRU-K）应重写该函数。
         */
        virtual void remove(Key key) {}

        /**
         * @brief 获取当前缓存的统计信息快照
         * @return CacheStats 包含命中、未命中和驱逐次数的结构体
         * @note 使用 load() 确保从原子变量中安全读取
         */
        CacheStats getStats() const
        {
            // 1.原子地读取当前值（线程安全）。比如 64 位计数在某些平台上不是天然单指令，普通读可能读到一半旧值一半新值（撕裂读取）。atomic 保证读到的是一个完整、有效的值。
            // 2.明确表达“这里是一次原子读操作”。
            // 所以“线程安全”不是说“读到全局一致快照”，而是说“这个读操作在并发下是合法且不会产生未定义行为”。
            return {hits_.load(), misses_.load(), evictions_.load()};
        }

        /**
         * @brief 重置所有统计计数器为零
         */
        void resetStats()
        {
            hits_ = 0;
            misses_ = 0;
            evictions_ = 0;
        }

    // 本类 + 派生类可访问，类外不行。
    /*
     * 为什么选 protected：
     * 派生类需要在 get/put 里更新计数（++this->hits_ 等）。
     * 但不希望外部直接随意改计数器。
     * 所以 protected 在这里是折中：对继承开放，对外部封装。
    */

    /*
     * 如果改成 private，派生类看不到 hits_，就不能直接改。那就要在基类提供“受保护的操作接口”
     * void recordHit() { ++hits_; } 之类的。这样虽然封装了计数器，但增加了接口复杂度和调用开销。
    
    
    */
    protected:
        /**
         * @brief 命中计数器
         * 使用 mutable 允许在 const 成员函数中修改，使用 atomic 保证线程安全
         * @note 派生类在 get() 实现(逻辑上被视为“读”操作，但仍想更新命中统计)中应根据结果增加 hits_ 或 misses_，在 put() 实现中应根据驱逐情况增加 evictions_
         */

         /* “接口会改统计值，那为什么还标 const？”
          * 逻辑 const
            从业务语义看，对外部可观察的核心数据不变（比如缓存内容、键值映射不变）。
            统计计数这种“元数据”变化通常不算改变对象核心语义。

            物理 const
            成员字节发生变化（计数器递增）是物理变化。
            const + mutable 就是告诉编译器：
            这个接口在逻辑上是只读；
            但允许修改某些“辅助状态”（统计、缓存命中记录、锁等）。
         */

        // C++ 原子类型，支持多线程下无数据竞争的读写增减
        // 特别适用于计数器这类共享状态
        mutable std::atomic<size_t> hits_{0};

        /**
         * @brief 未命中计数器
         */
        mutable std::atomic<size_t> misses_{0};

        /**
         * @brief 驱逐计数器（当缓存空间满触发清理时增加）
         */
        mutable std::atomic<size_t> evictions_{0};
    };
}