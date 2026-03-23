#pragma once

#include <cstdint>

/**
 * 轻量级快速随机数生成器（线性同余算法 LCG）
 * 特点：无锁、速度快、占用低，适合性能基准中的随机轨迹构造。
 */
class FastRng
{
public:
    /**
     * 构造函数
     * @param seed 随机数种子
     */
    explicit FastRng(uint64_t seed) : state_(seed) {}

    /**
     * 生成下一个随机数
     * @return 64位随机数
     */
    uint64_t next()
    {
        // LCG核心递推公式
        state_ = state_ * 6364136223846793005ULL + 1ULL;
        return state_;
    }

private:
    // 随机状态
    uint64_t state_;
};