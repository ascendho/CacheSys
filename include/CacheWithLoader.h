#pragma once

#include <functional>
#include <memory>
#include <stdexcept>

#include "CachePolicy.h"

namespace CacheSys
{
    // CacheWithLoader 将缓存与后端数据源（数据库、RPC、磁盘等）联动。
    // 实现经典的 Cache-Aside 模式：
    //   get 命中  → 直接返回缓存值
    //   get 未命中 → 调用 loader 从数据源加载，写入缓存后返回
    // loader 抛出异常时，get 会向上传播该异常，不缓存失败结果。
    template <typename Key, typename Value>
    class CacheWithLoader : public CachePolicy<Key, Value>
    {
    public:
        // loader : 接收 key，返回对应的 Value；缓存未命中时被调用
        using Loader = std::function<Value(const Key &)>;

        CacheWithLoader(std::unique_ptr<CachePolicy<Key, Value>> inner, Loader loader);

        // put : 直接写入缓存（绕过 loader）
        void put(Key key, Value value) override;
        // get : 命中→返回缓存；未命中→调用 loader 回填后返回
        bool get(Key key, Value &value) override;
        Value get(Key key) override;
        void remove(Key key) override;

        // 返回 loader 实际被调用的次数（即真实的后端请求数）
        size_t loaderCallCount() const { return loaderCalls_.load(); }

    private:
        std::unique_ptr<CachePolicy<Key, Value>> inner_;
        Loader                                   loader_;
        mutable std::atomic<size_t>              loaderCalls_{0};
    };
}

#include "../src/CacheWithLoader.tpp"
