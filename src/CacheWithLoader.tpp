#pragma once

#include "../include/CacheWithLoader.h"

namespace CacheSys
{
    /**
     * @brief CacheWithLoader 构造函数实现
     * @tparam Key 缓存键类型
     * @tparam Value 缓存值类型
     * @param inner 底层缓存策略实例（如LRU/LFU/ARC，通过unique_ptr传递所有权）
     * @param loader 缓存未命中时的「回源加载函数」（函数对象/lambda）
     * @details 初始化：接管底层缓存策略的所有权，保存回源加载逻辑
     */
    template <typename Key, typename Value>
    CacheWithLoader<Key, Value>::CacheWithLoader(
        std::unique_ptr<CachePolicy<Key, Value>> inner, Loader loader)
        : inner_(std::move(inner)),  // 转移底层缓存的所有权到inner_成员
          loader_(std::move(loader)) // 保存回源加载函数（支持任意可调用对象）
    {
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
     * @brief 向缓存中写入键值对
     * @tparam Key 缓存键类型
     * @tparam Value 缓存值类型
     * @param key 缓存键
     * @param value 缓存值
     * @details 直接委托给底层缓存策略的put方法，无额外逻辑
     */
    template <typename Key, typename Value>
    void CacheWithLoader<Key, Value>::put(Key key, Value value)
    {
        inner_->put(key, value);
    }

    /**
     * @brief 从缓存获取值（核心逻辑：自动回源）
     * @tparam Key 缓存键类型
     * @tparam Value 缓存值类型
     * @param key 要查询的缓存键
     * @param value 输出参数，存储查询到的值（无论缓存命中/回源加载）
     * @return bool 缓存命中状态：true=命中，false=未命中（但已通过loader回填缓存）
     * @details 核心流程：
     *  1. 先查底层缓存，命中则统计hits并返回true；
     *  2. 未命中则调用loader回源加载数据，统计loader调用次数；
     *  3. 将加载的数据写入缓存，统计misses，返回false（标记本次是miss）；
     *  4. 若loader抛出异常，会直接向上传播，且不会修改缓存（保证缓存数据安全）
     */
    template <typename Key, typename Value>
    bool CacheWithLoader<Key, Value>::get(Key key, Value &value)
    {
        // 第一步：查询底层缓存
        if (inner_->get(key, value))
        {
            // 缓存命中：统计命中次数，返回true
            ++this->hits_;
            return true;
        }

        // 第二步：缓存未命中 → 执行回源加载
        ++loaderCalls_;          // 统计回源加载的次数
        value = loader_(key);    // 调用loader函数从后端（如DB）加载数据
        inner_->put(key, value); // 将加载的数据写入缓存（回填）

        // 第三步：统计未命中次数，返回false（标记本次是miss）
        ++this->misses_;
        return false;
    }

    /**
     * @brief 简化版get：直接返回值（不返回命中状态）
     * @tparam Key 缓存键类型
     * @tparam Value 缓存值类型
     * @param key 要查询的缓存键
     * @return Value 缓存值（命中则返回缓存数据，未命中则返回loader加载的数据）
     * @details 调用带输出参数的get方法，适配「只关心值、不关心命中状态」的场景
     */
    template <typename Key, typename Value>
    Value CacheWithLoader<Key, Value>::get(Key key)
    {
        Value value{};         // 初始化默认值
        this->get(key, value); // 调用核心get逻辑
        return value;          // 返回最终值（缓存/回源加载的）
    }

    /**
     * @brief 从缓存中删除指定键
     * @tparam Key 缓存键类型
     * @tparam Value 缓存值类型
     * @param key 要删除的缓存键
     * @details 直接委托给底层缓存策略的remove方法
     */
    template <typename Key, typename Value>
    void CacheWithLoader<Key, Value>::remove(Key key)
    {
        inner_->remove(key);
    }
}