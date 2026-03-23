#pragma once

#include <string>
#include "CacheManager.h"

// 打印格式化分段标题（用于控制台展示）
void printSection(const std::string &title);

// 将策略枚举转为可读字符串
std::string policyName(CacheSys::CacheManager::PolicyType p);

// 模拟用户数据库：用于演示缓存回源场景
struct MockUserDB
{
    // 统计数据库查询次数
    int queryCount = 0;

    // 按用户ID查询模拟数据
    std::string query(const std::string &userId);
};

// 模拟商品数据库：用于演示热点与回源场景
struct MockProductDB
{
    // 统计数据库查询次数
    int queryCount = 0;

    // 按商品ID查询模拟数据
    std::string query(int productId);
};