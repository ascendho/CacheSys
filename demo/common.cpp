#include "common.h"

#include <iomanip>
#include <iostream>

void printSection(const std::string &title)
{
    std::cout << "\n┌─────────────────────────────────────────┐\n";
    std::cout << "│  " << std::left << std::setw(40) << title << "│\n";
    std::cout << "└─────────────────────────────────────────┘\n";
}

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
    return "UNKNOWN";
}

std::string MockUserDB::query(const std::string &userId)
{
    ++queryCount;
    std::cout << "    [DB] 查询用户 " << userId << " (第 " << queryCount << " 次 DB 访问)\n";
    return "User<" + userId + ">: name=张三_" + userId + ", role=admin";
}

std::string MockProductDB::query(int productId)
{
    ++queryCount;
    std::cout << "    [DB] 查询商品 #" << productId << " (第 " << queryCount << " 次 DB 访问)\n";
    return "Product<" + std::to_string(productId) + ">: price=" +
           std::to_string(productId * 10) + "元, stock=100";
}
