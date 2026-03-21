#pragma once

#include <string>

#include "CacheManager.h"

void printSection(const std::string &title);
std::string policyName(CacheSys::CacheManager::PolicyType p);

struct MockUserDB
{
    int queryCount = 0;
    std::string query(const std::string &userId);
};

struct MockProductDB
{
    int queryCount = 0;
    std::string query(int productId);
};
