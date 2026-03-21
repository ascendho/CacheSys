#pragma once

#include <cstddef>

namespace CacheSys::Eval::Scenario
{
    struct Operation
    {
        bool isPut = false;
        int key = 0;
        int value = 0;
    };

    struct EvalResult
    {
        size_t gets = 0;
        size_t hits = 0;
    };
}
