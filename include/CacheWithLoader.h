#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
#include <atomic>

#include "CachePolicy.h"

namespace CacheSys
{
    /**
     * @class CacheWithLoader
     * @brief 带有自动加载功能的缓存装饰器 (Read-Through Cache Pattern)
     * @tparam Key 键类型
     * @tparam Value 值类型
     * 
     * 该类包装了另一个缓存策略（inner_），并为其增加了“自动加载”功能。
     * 当尝试获取一个不存在于缓存中的键时，它会透明地调用 Loader 函数获取数据，
     * 并将结果自动存入缓存。
     */
    template <typename Key, typename Value>
    class CacheWithLoader : public CachePolicy<Key, Value>
    {
    public:
        /**
         * @brief 加载器函数类型别名
         * 接收一个 Key，返回对应的 Value。
         * 通常是一个 Lambda 表达式或函数指针，内部包含从数据库/网络读取数据的逻辑。
         */
        using Loader = std::function<Value(const Key &)>;

        /**
         * @brief 构造函数
         * @param inner 实际负责存储的底层缓存策略实例（如 LruCache 的智能指针）
         * @param loader 当缓存未命中时调用的函数
         * @throw std::invalid_argument 如果 inner 或 loader 为空则抛出异常
         */
        CacheWithLoader(std::unique_ptr<CachePolicy<Key, Value>> inner, Loader loader);

        /**
         * @brief 存入数据（手动更新）
         * 直接透传给底层 inner_ 缓存。
         */
        void put(Key key, Value value) override;
        
        /**
         * @brief 获取数据（引用方式）
         * 1. 尝试从 inner_ 获取数据。
         * 2. 如果 inner_ 未命中，则调用 loader_ 加载数据。
         * 3. 将加载后的数据存入 inner_，并增加 loaderCalls_ 计数。
         * @return bool 总是返回 true（除非 loader 抛出异常），因为它保证了数据会被加载。
         */
        bool get(Key key, Value &value) override;

        /**
         * @brief 获取数据（直接返回方式）
         * 内部调用 get(Key, Value&) 实现。
         * @return Value 缓存中的值或新加载的值。
         */
        Value get(Key key) override;

        /**
         * @brief 显式移除某个键
         */
        void remove(Key key) override;

        /**
         * @brief 获取加载器被触发的总次数
         * 该指标可以用来评估外部数据源（如数据库）的访问压力。
         * @return size_t 加载次数
         */
        size_t loaderCallCount() const { return loaderCalls_.load(); }

    private:
        /// 被包装的底层缓存策略 (LRU/LFU/ARC)
        std::unique_ptr<CachePolicy<Key, Value>> inner_;
        
        /// 用户定义的外部加载逻辑
        Loader                                   loader_;
        
        /// 原子计数器，记录触发加载器获取数据的次数
        mutable std::atomic<size_t>              loaderCalls_{0};
    };
}

// 包含模板的具体实现
#include "../src/CacheWithLoader.tpp"