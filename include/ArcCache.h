#pragma once

#include <memory>

#include "CachePolicy.h"
#include "detail/ArcLfuPart.h"
#include "detail/ArcLruPart.h"

namespace CacheSys
{
    /**
     * @brief ARC (Adaptive Replacement Cache) 缓存策略实现类
     * @tparam Key 缓存键的类型，要求可哈希、可比较
     * @tparam Value 缓存值的类型
     * @details ARC 是一种自适应的缓存替换算法，结合了 LRU (最近最少使用)
     * 和 LFU (最不经常使用) 的优点，能够根据访问模式动态调整替换策略，
     * 比单纯的 LRU/LFU 具有更高的缓存命中率
     */
    template <typename Key, typename Value>
    class ArcCache : public CachePolicy<Key, Value>
    {
    public:
        /**
         * @brief 构造函数，初始化 ARC 缓存
         * @param capacity 缓存的总容量（最大可存储的键值对数量），默认值 10
         * @param transformThreshold LRU/LFU 部分转换的阈值，默认值 2
         */
        ArcCache(size_t capacity = 10, size_t transformThreshold = 2);

        /**
         * @brief 析构函数，使用 override 确保重写基类析构函数，默认实现
         */
        ~ArcCache() override = default;

        /**
         * @brief 向缓存中添加/更新键值对
         * @param key 要添加/更新的缓存键
         * @param value 对应的缓存值
         * @override 重写自 CachePolicy 基类的纯虚函数
         */
        void put(Key key, Value value) override;

        /**
         * @brief 从缓存中获取指定键的值（通过引用输出）
         * @param key 要查找的缓存键
         * @param value 输出参数，用于存储找到的缓存值
         * @return bool 查找结果：true 表示找到并将值存入 value，false 表示未找到
         * @override 重写自 CachePolicy 基类的纯虚函数
         */
        bool get(Key key, Value &value) override;

        /**
         * @brief 从缓存中获取指定键的值（直接返回）
         * @param key 要查找的缓存键
         * @return Value 找到的缓存值；若未找到，行为由具体实现决定（通常返回默认值或抛出异常）
         * @override 重写自 CachePolicy 基类的纯虚函数
         */
        Value get(Key key) override;

    private:
        /**
         * @brief 当命中幽灵缓存（ghost cache）时，重新平衡 LRU/LFU 缓存部分
         * @param key 命中的幽灵缓存键
         * @return bool 重平衡操作是否成功
         * @details 幽灵缓存用于记录近期被淘汰的缓存项，命中时触发缓存结构的调整，
         * 以优化后续的缓存替换策略
         */
        bool rebalanceByGhostHit(Key key);

        /**
         * @brief 向后兼容的包装函数，检查幽灵缓存是否命中
         * @param key 要检查的缓存键
         * @return bool 幽灵缓存是否命中
         * @details 为了兼容旧版本代码，封装了 rebalanceByGhostHit 方法，
         * 本质上直接调用 rebalanceByGhostHit
         */
        bool checkGhostCaches(Key key) { return rebalanceByGhostHit(key); }

    private:
        size_t capacity_;                                 // 缓存的总容量（最大可存储的键值对数量）
        size_t transformThreshold_;                       // LRU/LFU 部分转换的阈值，控制策略切换的条件
        std::unique_ptr<ArcLruPart<Key, Value>> lruPart_; // 管理 ARC 缓存中 LRU 部分的智能指针
        std::unique_ptr<ArcLfuPart<Key, Value>> lfuPart_; // 管理 ARC 缓存中 LFU 部分的智能指针
    };
}

#include "../src/ArcCache.tpp"