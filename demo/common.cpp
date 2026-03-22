#include "common.h"           
#include <iomanip>            // 用于格式化输出（如std::setw）
#include <iostream>           

// 打印带边框的分段标题
void printSection(const std::string &title)
{
    std::cout << "\n┌─────────────────────────────────────────┐\n";
    std::cout << "│  " << std::left << std::setw(40) << title << "│\n";
    std::cout << "└─────────────────────────────────────────┘\n";
}

// 根据缓存策略枚举值，返回对应的名称字符串
std::string policyName(CacheSys::CacheManager::PolicyType p)
{
    switch (p)
    {
    case CacheSys::CacheManager::PolicyType::LRU:
        return "LRU";
    case CacheSys::CacheManager::PolicyType::LFU:
        return "LFU";
    case CacheSys::CacheManager::PolicyType::ARC:
        return "ARC";
    }

    // 未知策略返回默认值
    return "UNKNOWN";  
}

// 模拟用户数据库查询接口：统计查询次数，输出查询日志并返回模拟用户数据
std::string MockUserDB::query(const std::string &userId)
{
    // 累计查询次数
    ++queryCount;  

    std::cout << "    [DB] 查询用户 " << userId << " (第 " << queryCount << " 次 DB 访问)\n";
    return "User<" + userId + ">: name=张三_" + userId + ", role=admin";
}

// 模拟产品数据库查询接口：统计查询次数，输出查询日志并返回模拟产品数据
std::string MockProductDB::query(int productId)
{
    // 累计查询次数
    ++queryCount;  
    
    std::cout << "    [DB] 查询商品 #" << productId << " (第 " << queryCount << " 次 DB 访问)\n";
    return "Product<" + std::to_string(productId) + ">: price=" +
           std::to_string(productId * 10) + "元, stock=100";
}