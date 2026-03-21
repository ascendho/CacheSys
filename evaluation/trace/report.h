#pragma once

#include <cstddef>
#include <string>

#include "types.h"

namespace CacheSys::Eval::Trace
{
    void printHelp(const char *prog);
    void printHeader();
    void printRow(const std::string &traceName,
                  int capacity,
                  const std::string &policy,
                  size_t accesses,
                  const EvalResult &result,
                  double deltaOpt);
}
