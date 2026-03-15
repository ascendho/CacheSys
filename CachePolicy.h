#pragma once

/**
 * @namespace CacheSys
 * @brief 缓存系统核心命名空间，封装缓存策略接口及各类具体缓存实现（如LRU/LFU/ARC）
 * @note 该命名空间下所有类均遵循面向对象的接口隔离原则，保证缓存策略的可替换性
 */
namespace CacheSys
{
    /**
     * @class CachePolicy
     * @brief 缓存策略抽象基类（接口类），定义所有缓存策略的统一操作规范
     * @tparam Key 缓存键的类型（需支持相等比较，若用于哈希表需支持哈希运算）
     * @tparam Value 缓存值的类型（支持任意可拷贝/赋值的类型）
     * @设计模式 接口模式：通过纯虚函数定义接口，派生类实现具体缓存策略（LRU/LFU/ARC等）
     * @note 所有成员函数均为纯虚函数，派生类必须实现；虚析构保证基类指针正确析构派生类对象
     */
    template <typename Key, typename Value>
    class CachePolicy
    {
    public:
        /**
         * @brief 虚析构函数（默认实现）
         * @关键设计 虚析构的必要性：当通过基类指针（CachePolicy*）销毁派生类对象（如LruCache）时，
         *          能保证派生类的析构函数被正确调用，避免内存泄漏
         */
        virtual ~CachePolicy() = default;

        /**
         * @brief 纯虚函数：添加/更新缓存项
         * @param key 缓存键（唯一标识缓存项）
         * @param value 缓存值（要存储/更新的内容）
         * @note 派生类需实现该接口，保证：
         *       1. 若key已存在，更新对应value；
         *       2. 若key不存在，添加新缓存项；
         *       3. 缓存满时按自身策略淘汰旧项（如LRU淘汰最久未使用项）
         */
        virtual void put(Key key, Value value) = 0;

        /**
         * @brief 纯虚函数：获取缓存项（通过引用返回值，推荐优先使用）
         * @param key 要查找的缓存键
         * @param value 输出参数：找到缓存项则赋值，未找到则保持原值
         * @return bool 查找结果：true=找到，false=未找到
         * @note 优势：避免复杂类型（如std::string）的默认构造/拷贝开销，效率更高
         */
        virtual bool get(Key key, Value &value) = 0;

        /**
         * @brief 纯虚函数：重载get方法（直接返回缓存值）
         * @param key 要查找的缓存键
         * @return Value 查找结果：找到则返回对应值，未找到则返回Value的默认构造值
         * @note 适用场景：简单类型（如int/float），复杂类型建议使用带引用的get方法
         * @设计依赖 派生类通常基于带引用的get方法实现该接口，保证逻辑一致性
         */
        virtual Value get(Key key) = 0;
    };
    
}