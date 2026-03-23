#pragma once

#include <cstddef>

/**
 * @brief 缓存评估与场景模拟命名空间
 * 
 * 该命名空间包含了用于测试缓存性能的辅助结构，如操作指令定义和结果统计。
 */
namespace CacheSys::Eval::Scenario
{
    /**
     * @struct Operation
     * @brief 缓存操作指令结构体
     * 
     * 用于预定义一系列缓存操作（Workload），以便在评估时回放。
     */
    struct Operation
    {
        /**
         * @brief 操作类型标识
         * true 表示执行 put（存入）操作；
         * false 表示执行 get（获取）操作。
         */
        bool isPut = false; 

        /**
         * @brief 操作对应的键（Key）
         */
        int key = 0;        

        /**
         * @brief 操作对应的值（Value）
         * 仅在 isPut 为 true 时有效；在 get 操作中通常被忽略。
         */
        int value = 0;      
    };

    /**
     * @struct EvalResult
     * @brief 评估结果统计结构体
     * 
     * 用于存储在一段测试场景执行后的汇总数据。
     */
    struct EvalResult
    {
        /**
         * @brief 总读取请求次数
         * 记录场景中一共发起了多少次 get 操作。
         */
        size_t gets = 0; 

        /**
         * @brief 缓存命中次数
         * 记录在所有 get 操作中，有多少次成功从缓存中找到了数据。
         */
        size_t hits = 0; 
        
        /**
         * @brief 获取当前场景的命中率
         * @return double 命中率 (0.0 ~ 1.0)
         */
        double hitRate() const {
            return gets > 0 ? static_cast<double>(hits) / gets : 0.0;
        }
    };
}