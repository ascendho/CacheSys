#pragma once

#include <string>
#include "CacheManager.h"

// 打印格式化的分段标题（用于控制台输出分隔模块）
void printSection(const std::string &title);

// 将缓存策略枚举转换为可读的名称字符串（如LRU -> "LRU"）
std::string policyName(CacheSys::CacheManager::PolicyType p);

// 模拟用户数据库（用于缓存测试，统计查询次数）
struct MockUserDB
{
    // 累计查询次数（用于计算缓存命中率）
    int queryCount = 0;

    // 查询指定用户ID的模拟用户信息
    std::string query(const std::string &userId);
};

// 模拟产品数据库（用于缓存测试，统计查询次数）
struct MockProductDB
{
    // 累计查询次数（用于计算缓存命中率）
    int queryCount = 0;

    // 查询指定产品ID的模拟产品信息
    std::string query(int productId);
};