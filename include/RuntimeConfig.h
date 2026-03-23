#pragma once

#include <chrono>
#include <cctype>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "ArcCache.h"
#include "CacheManager.h"
#include "CacheWithLoader.h"
#include "LfuCache.h"
#include "LruCache.h"
#include "TtlCache.h"

namespace CacheSys
{
    /**
     * @struct CacheInstanceConfig
     * @brief 单个缓存实例的配置参数
     */
    struct CacheInstanceConfig
    {
        std::string name;                                        ///< 缓存唯一标识名称
        CacheManager::PolicyType policy = CacheManager::PolicyType::LRU; ///< 淘汰策略
        int capacity = 0;                                        ///< 缓存最大容量
        int ttlMs = 0;                                           ///< 生存时间 (毫秒)，0 表示不限制
        bool enableLoader = false;                               ///< 是否启用自动加载功能
    };

    /**
     * @class RuntimeConfig
     * @brief 运行时配置解析器
     * 
     * 负责从文本文件中读取并解析缓存配置信息。
     * 支持格式示例： cache name=user_ptr policy=lru capacity=1000 ttl_ms=5000 loader=true
     */
    struct RuntimeConfig
    {
        std::vector<CacheInstanceConfig> instances; ///< 解析出的所有缓存配置项

        /**
         * @brief 从指定文件加载配置
         * @param path 配置文件路径
         * @return RuntimeConfig 包含所有实例配置的对象
         * @throw std::runtime_error 如果文件无法打开或解析失败
         */
        static RuntimeConfig loadFromFile(const std::string &path)
        {
            std::ifstream in(path);
            if (!in)
            {
                throw std::runtime_error("RuntimeConfig: cannot open file: " + path);
            }

            RuntimeConfig cfg;
            std::string line;
            size_t lineNo = 0;

            while (std::getline(in, line))
            {
                ++lineNo;
                const std::string t = trim(line);
                // 忽略空行和以 # 开头的注释
                if (t.empty() || t[0] == '#')
                {
                    continue;
                }

                CacheInstanceConfig item;
                bool hasName = false;
                bool hasPolicy = false;
                bool hasCapacity = false;

                std::istringstream iss(t);
                std::string token;
                while (iss >> token)
                {
                    // 忽略前导关键字 "cache"
                    if (toLower(token) == "cache")
                    {
                        continue;
                    }

                    const auto pos = token.find('=');
                    if (pos == std::string::npos)
                    {
                        throw std::runtime_error("RuntimeConfig: invalid token at line " +
                                                 std::to_string(lineNo) + ": " + token);
                    }

                    const std::string key = toLower(token.substr(0, pos));
                    const std::string val = token.substr(pos + 1);

                    // 映射配置项到结构体字段
                    if (key == "name")
                    {
                        item.name = val;
                        hasName = !item.name.empty();
                    }
                    else if (key == "policy")
                    {
                        item.policy = parsePolicy(val, lineNo);
                        hasPolicy = true;
                    }
                    else if (key == "capacity")
                    {
                        item.capacity = parseInt(val, "capacity", lineNo);
                        hasCapacity = true;
                    }
                    else if (key == "ttl_ms")
                    {
                        item.ttlMs = parseInt(val, "ttl_ms", lineNo);
                    }
                    else if (key == "loader")
                    {
                        item.enableLoader = parseBool(val, lineNo);
                    }
                    else
                    {
                        throw std::runtime_error("RuntimeConfig: unknown key at line " +
                                                 std::to_string(lineNo) + ": " + key);
                    }
                }

                // 强制校验必填参数
                if (!hasName || !hasPolicy || !hasCapacity)
                {
                    throw std::runtime_error("RuntimeConfig: line " + std::to_string(lineNo) +
                                             " must include name/policy/capacity");
                }
                if (item.capacity <= 0)
                {
                    throw std::runtime_error("RuntimeConfig: capacity must be > 0 at line " +
                                             std::to_string(lineNo));
                }
                if (item.ttlMs < 0)
                {
                    throw std::runtime_error("RuntimeConfig: ttl_ms must be >= 0 at line " +
                                             std::to_string(lineNo));
                }

                cfg.instances.push_back(item);
            }

            return cfg;
        }

    private:
        /** @brief 去除字符串两端的空白字符 */
        static std::string trim(const std::string &s)
        {
            size_t start = 0;
            while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
            {
                ++start;
            }

            size_t end = s.size();
            while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
            {
                --end;
            }
            return s.substr(start, end - start);
        }

        /** @brief 字符串转小写 */
        static std::string toLower(std::string s)
        {
            for (char &c : s)
            {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return s;
        }

        /** @brief 解析整数，带严格的格式校验 */
        static int parseInt(const std::string &v, const std::string &field, size_t lineNo)
        {
            try
            {
                size_t pos = 0;
                int x = std::stoi(v, &pos);
                if (pos != v.size())
                {
                    throw std::runtime_error("trailing chars");
                }
                return x;
            }
            catch (...)
            {
                throw std::runtime_error("RuntimeConfig: invalid integer for " + field +
                                         " at line " + std::to_string(lineNo));
            }
        }

        /** @brief 解析布尔值，支持多种文本表示形式 */
        static bool parseBool(const std::string &v, size_t lineNo)
        {
            const std::string x = toLower(v);
            if (x == "1" || x == "true" || x == "yes" || x == "on")
            {
                return true;
            }
            if (x == "0" || x == "false" || x == "no" || x == "off")
            {
                return false;
            }
            throw std::runtime_error("RuntimeConfig: invalid bool at line " + std::to_string(lineNo));
        }

        /** @brief 解析缓存策略关键字 */
        static CacheManager::PolicyType parsePolicy(const std::string &v, size_t lineNo)
        {
            const std::string x = toLower(v);
            if (x == "lru")
            {
                return CacheManager::PolicyType::LRU;
            }
            if (x == "lfu")
            {
                return CacheManager::PolicyType::LFU;
            }
            if (x == "arc")
            {
                return CacheManager::PolicyType::ARC;
            }
            throw std::runtime_error("RuntimeConfig: invalid policy at line " + std::to_string(lineNo));
        }
    };

    /**
     * @class RuntimeAssembler
     * @brief 运行时装配器
     * 
     * 负责将解析后的配置项转化成实际的缓存对象。
     * 核心逻辑是基于装饰器模式层层包装缓存对象。
     */
    class RuntimeAssembler
    {
    public:
        using Key = std::string;
        using Value = std::string;
        using CachePtr = std::shared_ptr<CachePolicy<Key, Value>>;
        using Loader = std::function<Value(const Key &)>;
        using LoaderRegistry = std::unordered_map<std::string, Loader>;

        /**
         * @brief 根据配置构建缓存实例列表
         * @param config 解析后的 RuntimeConfig
         * @param loaders (可选) 外部注册的加载器函数映射表
         * @return std::unordered_map<std::string, CachePtr> 构建好的缓存对象映射表
         * 
         * 装配逻辑：
         * 1. 根据 policy 创建基础缓存（LRU/LFU/ARC）。
         * 2. 若启用 Loader，使用 CacheWithLoader 进行第一层包装。
         * 3. 若设置了 ttlMs，使用 TtlCache 进行第二层包装。
         */
        static std::unordered_map<std::string, CachePtr>
        build(const RuntimeConfig &config, const LoaderRegistry &loaders = {})
        {
            std::unordered_map<std::string, CachePtr> result;
            result.reserve(config.instances.size());

            for (const auto &item : config.instances)
            {
                if (result.count(item.name) > 0)
                {
                    throw std::runtime_error("RuntimeAssembler: duplicate cache name: " + item.name);
                }

                // 步骤 1: 创建基础淘汰策略实例
                std::unique_ptr<CachePolicy<Key, Value>> cache = makeBase(item);

                // 步骤 2: 装饰自动加载功能
                if (item.enableLoader)
                {
                    Loader loader;
                    auto it = loaders.find(item.name);
                    if (it != loaders.end())
                    {
                        loader = it->second; // 使用传入的自定义加载逻辑
                    }
                    else
                    {
                        // 默认加载逻辑：仅生成占位符字符串
                        loader = [cacheName = item.name](const Key &key)
                        {
                            return std::string("AUTO<") + cacheName + ">:" + key;
                        };
                    }

                    // 包装装饰器
                    cache = std::make_unique<CacheWithLoader<Key, Value>>(std::move(cache), std::move(loader));
                }

                // 步骤 3: 装饰过期时间功能
                if (item.ttlMs > 0)
                {
                    cache = std::make_unique<TtlCache<Key, Value>>(
                        std::move(cache),
                        std::chrono::milliseconds(item.ttlMs));
                }

                // 存入结果集
                result.emplace(item.name, CachePtr(std::move(cache)));
            }

            return result;
        }

    private:
        /**
         * @brief 工厂方法：根据配置创建底层缓存策略
         */
        static std::unique_ptr<CachePolicy<Key, Value>> makeBase(const CacheInstanceConfig &item)
        {
            switch (item.policy)
            {
            case CacheManager::PolicyType::LRU:
                return std::make_unique<LruCache<Key, Value>>(item.capacity);
            case CacheManager::PolicyType::LFU:
                return std::make_unique<LfuCache<Key, Value>>(item.capacity);
            case CacheManager::PolicyType::ARC:
                return std::make_unique<ArcCache<Key, Value>>(static_cast<size_t>(item.capacity));
            }

            throw std::runtime_error("RuntimeAssembler: unsupported policy");
        }
    };
}