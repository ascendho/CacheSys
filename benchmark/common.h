#pragma once

#include <cstdint>

class FastRng
{
public:
    explicit FastRng(uint64_t seed) : state_(seed) {}

    uint64_t next()
    {
        state_ = state_ * 6364136223846793005ULL + 1ULL;
        return state_;
    }

private:
    uint64_t state_;
};
