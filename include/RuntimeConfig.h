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
    struct CacheInstanceConfig
    {
        std::string name;
        CacheManager::PolicyType policy = CacheManager::PolicyType::LRU;
        int capacity = 0;
        int ttlMs = 0;
        bool enableLoader = false;
    };

    struct RuntimeConfig
    {
        std::vector<CacheInstanceConfig> instances;

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

        static std::string toLower(std::string s)
        {
            for (char &c : s)
            {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return s;
        }

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

    class RuntimeAssembler
    {
    public:
        using Key = std::string;
        using Value = std::string;
        using CachePtr = std::shared_ptr<CachePolicy<Key, Value>>;
        using Loader = std::function<Value(const Key &)>;
        using LoaderRegistry = std::unordered_map<std::string, Loader>;

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

                std::unique_ptr<CachePolicy<Key, Value>> cache = makeBase(item);

                if (item.enableLoader)
                {
                    Loader loader;
                    auto it = loaders.find(item.name);
                    if (it != loaders.end())
                    {
                        loader = it->second;
                    }
                    else
                    {
                        loader = [cacheName = item.name](const Key &key)
                        {
                            return std::string("AUTO<") + cacheName + ">:" + key;
                        };
                    }

                    cache = std::make_unique<CacheWithLoader<Key, Value>>(std::move(cache), std::move(loader));
                }

                if (item.ttlMs > 0)
                {
                    cache = std::make_unique<TtlCache<Key, Value>>(
                        std::move(cache),
                        std::chrono::milliseconds(item.ttlMs));
                }

                result.emplace(item.name, CachePtr(std::move(cache)));
            }

            return result;
        }

    private:
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
