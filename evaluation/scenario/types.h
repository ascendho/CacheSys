#pragma once

#include <cstddef>

// Eval：评估模块 → Scenario：场景化测试子模块（模拟真实业务操作序列）
namespace CacheSys::Eval::Scenario
{
    /**
     * 缓存操作结构体（描述单次PUT/GET操作）
     * 用于构建场景化测试的操作序列
     */
    struct Operation
    {
        bool isPut = false; // 是否为PUT操作（false则为GET操作）
        int key = 0;        // 操作的缓存key
        int value = 0;      // PUT操作的value（GET操作时该字段无意义）
    };

    /**
     * 场景化测试评估结果结构体
     * 统计GET操作的核心命中指标
     */
    struct EvalResult
    {
        size_t gets = 0; // GET操作总次数（仅统计GET，排除PUT）
        size_t hits = 0; // GET操作命中次数
    };
}