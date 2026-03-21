#pragma once

#include <vector>

#include "types.h"

namespace CacheSys::Eval::Scenario
{
    EvalResult evalLru(const std::vector<Operation> &ops, int capacity);
    EvalResult evalLfu(const std::vector<Operation> &ops, int capacity);
    EvalResult evalArc(const std::vector<Operation> &ops, int capacity);
}
