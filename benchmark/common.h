#pragma once

#include <cstdint>

/**
 * 轻量级快速随机数生成器（线性同余算法 LCG）
 * 核心特点：无锁、运算快、内存占用极低，适合缓存轨迹模拟等非加密场景
 * 固定种子可生成完全一致的随机序列，便于测试结果复现
 */
class FastRng
{
public:
    /**
     * 构造函数：初始化随机数种子
     * @param seed 64位无符号种子值，决定随机序列的起始状态
     */
    explicit FastRng(uint64_t seed) : state_(seed) {}

    /**
     * 生成下一个64位无符号随机数
     * 采用优化的LCG参数，兼顾随机性和运算速度
     * @return 64位无符号随机数
     */
    uint64_t next()
    {
        // LCG核心公式：state = state * 乘数 + 增量（固定优化参数，保证分布均匀）
        state_ = state_ * 6364136223846793005ULL + 1ULL;
        return state_;
    }

private:
    // 随机数生成器的内部状态（种子+累计计算值，核心依赖）
    uint64_t state_;
};