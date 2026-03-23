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
         * @brief 计算当前的缓存命中率
         * @return 命中率 (0.0 到 1.0)，如果总请求为0则返回 0.0
         */
        double hitRate() const
        {
            size_t total = hits + misses;
            return total ? static_cast<double>(hits) / total : 0.0;
        }

        /**
         * @brief 获取总请求次数（命中 + 未命中）
         * @return 总请求次数
         */
        size_t totalRequests() const { return hits + misses; }
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
         */
        virtual void remove(Key key) {}

        /**
         * @brief 获取当前缓存的统计信息快照
         * @return CacheStats 包含命中、未命中和驱逐次数的结构体
         * @note 使用 load() 确保从原子变量中安全读取
         */
        CacheStats getStats() const
        {
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

    protected:
        /**
         * @brief 命中计数器
         * 使用 mutable 允许在 const 成员函数中修改，使用 atomic 保证线程安全
         */
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