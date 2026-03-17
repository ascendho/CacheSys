#pragma once

#include "../include/CacheWithLoader.h"

namespace CacheSys
{
    template <typename Key, typename Value>
    CacheWithLoader<Key, Value>::CacheWithLoader(
        std::unique_ptr<CachePolicy<Key, Value>> inner, Loader loader)
        : inner_(std::move(inner)), loader_(std::move(loader))
    {
    }

    template <typename Key, typename Value>
    void CacheWithLoader<Key, Value>::put(Key key, Value value)
    {
        inner_->put(key, value);
    }

    template <typename Key, typename Value>
    bool CacheWithLoader<Key, Value>::get(Key key, Value &value)
    {
        // 先查缓存
        if (inner_->get(key, value))
        {
            ++this->hits_;
            return true;
        }

        // 缓存未命中：调用 loader 从后端加载
        ++loaderCalls_;
        value = loader_(key);   // 若 loader 抛出异常，向上传播，不修改缓存
        inner_->put(key, value);

        ++this->misses_;
        return false;   // 返回 false 表示本次是缓存 miss（即使已经回填）
    }

    template <typename Key, typename Value>
    Value CacheWithLoader<Key, Value>::get(Key key)
    {
        Value value{};
        this->get(key, value);
        return value;
    }

    template <typename Key, typename Value>
    void CacheWithLoader<Key, Value>::remove(Key key)
    {
        inner_->remove(key);
    }
}
