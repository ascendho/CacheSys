#include "report.h"

#include <iomanip>
#include <iostream>

namespace CacheSys::Eval::Trace
{
    void printHelp(const char *prog)
    {
        std::cout << "Usage: " << prog << " [--trace-file=PATH] [--capacities=64,128,256]\n";
        std::cout << "Without --trace-file, built-in synthetic traces are used.\n";
    }

    void printHeader()
    {
        std::cout << "\nTrace-Driven Cache Policy Comparison (Miss Ratio)\n";
        std::cout << "--------------------------------------------------------------------------------\n";
        std::cout << std::left << std::setw(18) << "Trace"
                  << std::setw(10) << "Capacity"
                  << std::setw(8) << "Policy"
                  << std::setw(12) << "Accesses"
                  << std::setw(12) << "Misses"
                  << std::setw(14) << "MissRatio(%)"
                  << std::setw(14) << "DeltaOPT(pp)" << "\n";
        std::cout << "--------------------------------------------------------------------------------\n";
    }

    void printRow(const std::string &traceName,
                  int capacity,
                  const std::string &policy,
                  size_t accesses,
                  const EvalResult &result,
                  double deltaOpt)
    {
        std::cout << std::left << std::setw(18) << traceName
                  << std::setw(10) << capacity
                  << std::setw(8) << policy
                  << std::setw(12) << accesses
                  << std::setw(12) << result.misses
                  << std::setw(14) << std::fixed << std::setprecision(3) << (result.missRatio * 100.0)
                  << std::setw(14) << std::fixed << std::setprecision(3) << deltaOpt << "\n";
    }
}
