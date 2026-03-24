#include "common.h"           
#include <iomanip>            
#include <iostream>           

// 打印带边框的分段标题
void printSection(const std::string &title)
{
    std::cout << "\n┌─────────────────────────────────────────┐\n";
    std::cout << "│  " << std::left << std::setw(40) << title << "│\n";
    std::cout << "└─────────────────────────────────────────┘\n";
}

// 模拟用户数据库查询：累计计数并返回组装数据
std::string MockUserDB::query(const std::string &userId)
{
    // 统计DB调用次数
    ++queryCount;  

    std::cout << "    [DB] 查询用户 " << userId << " (第 " << queryCount << " 次 DB 访问)\n";
    return "User<" + userId + ">: name=张三_" + userId + ", role=admin";
}

// 模拟商品数据库查询：累计计数并返回组装数据
std::string MockProductDB::query(int productId)
{
    // 统计DB调用次数
    ++queryCount;  
    
    std::cout << "    [DB] 查询商品 #" << productId << " (第 " << queryCount << " 次 DB 访问)\n";
    return "Product<" + std::to_string(productId) + ">: price=" +
           std::to_string(productId * 10) + "元, stock=100";
}