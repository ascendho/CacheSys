#pragma once

#include "../include/CacheWithLoader.h"

namespace CacheSys
{
    /**
     * @brief 构造函数
     * @param inner 实际负责存储的底层缓存策略实例（所有权转移至本类）
     * @param loader 用户定义的加载器函数（通常是数据库查询或 API 调用）
     * 
     * @throw std::invalid_argument 如果 inner 缓存指针为空，或者 loader 函数不可调用。
     */
    template <typename Key, typename Value>
    CacheWithLoader<Key, Value>::CacheWithLoader(
        std::unique_ptr<CachePolicy<Key, Value>> inner, Loader loader)
        : inner_(std::move(inner)),  
          loader_(std::move(loader)) 
    {
        // 防御性编程：确保装饰器模式的底层组件有效
        if (!inner_)
        {
            throw std::invalid_argument("CacheWithLoader: inner cache must not be null");
        }
        if (!loader_)
        {
            throw std::invalid_argument("CacheWithLoader: loader must be callable");
        }
    }

    /**
     * @brief 手动放入数据
     * 直接透传给底层缓存。通常用于预热缓存或手动更新数据。
     */
    template <typename Key, typename Value>
    void CacheWithLoader<Key, Value>::put(Key key, Value value)
    {
        inner_->put(key, value);
    }

    /**
     * @brief 核心方法：带自动加载的获取逻辑 (Read-Through)
     * 
     * 执行流程：
     * 1. 尝试从底层物理缓存 (inner_) 中查找数据。
     * 2. 如果命中 (Hit)：增加命中计数，直接返回结果。
     * 3. 如果未命中 (Miss)：
     *    a. 增加加载器调用计数 (loaderCalls_)。
     *    b. 调用用户提供的 loader_ 函数从外部源获取数据。
     *    c. 将新获取的数据回填 (Refill) 到底层缓存中，以便下次直接命中。
     *    d. 增加未命中计数。
     * 
     * @return bool 虽然最终肯定能拿到数据，但如果初次查询是从外部加载的，则返回 false（表示缓存未命中）。
     */
    template <typename Key, typename Value>
    bool CacheWithLoader<Key, Value>::get(Key key, Value &value)
    {
        // 步骤 1：检查底层缓存
        if (inner_->get(key, value))
        {
            // 命中逻辑
            ++this->hits_;
            return true;
        }

        // 步骤 2：缓存未命中，执行自动加载逻辑
        ++loaderCalls_;             // 记录外部加载次数
        value = loader_(key);       // 阻塞调用加载器函数获取数据
        inner_->put(key, value);    // 将获取到的数据“回填”缓存

        // 统计：尽管现在 value 有值了，但在缓存层级上这属于一次未命中
        ++this->misses_;
        return false;
    }

    /**
     * @brief 获取数据（直接返回方式）
     * 
     * 即使底层缓存没有，也会通过 loader 加载并返回最新值。
     * @return Value 最终获取到的值。
     */
    template <typename Key, typename Value>
    Value CacheWithLoader<Key, Value>::get(Key key)
    {
        Value value{};         
        this->get(key, value); // 调用上面的 bool get() 逻辑
        return value;          
    }

    /**
     * @brief 移除数据
     * 同步移除底层缓存中的对应项。
     */
    template <typename Key, typename Value>
    void CacheWithLoader<Key, Value>::remove(Key key)
    {
        inner_->remove(key);
    }
}