#include "generators.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

// Eval：评估模块 → Trace：轨迹生成/解析工具实现
namespace CacheSys::Eval::Trace
{
    // 匿名命名空间：仅当前编译单元可见的轻量随机数生成器
    namespace
    {
        struct FastRng
        {
            // 构造函数：初始化随机种子
            explicit FastRng(uint64_t seed) : state(seed) {}

            // 线性同余法生成下一随机数（速度快，适合模拟场景）
            uint64_t next()
            {
                state = state * 6364136223846793005ULL + 1ULL;
                return state;
            }

            // 内部随机状态
            uint64_t state;
        };
    }

    // 解析CSV格式容量列表（如 "64,128,256"）
    std::vector<int> parseCapacities(const std::string &csv)
    {
        std::vector<int> capacities;

        // 使用字符串流按逗号拆分
        std::stringstream ss(csv);
        std::string item;

        // 逐项转换为整数并写入结果
        while (std::getline(ss, item, ','))
        {
            if (!item.empty())
            {
                capacities.push_back(std::stoi(item));
            }
        }

        return capacities;
    }

    // 从文件加载访问轨迹（每行一个整数key）
    std::vector<int> loadTraceFromFile(const std::string &path)
    {
        std::ifstream ifs(path);

        // 文件打开失败直接抛异常
        if (!ifs)
        {
            throw std::runtime_error("Failed to open trace file: " + path);
        }

        std::vector<int> trace;
        int key = 0;

        // 顺序读取所有key
        while (ifs >> key)
        {
            trace.push_back(key);
        }

        return trace;
    }

    // 生成循环扫描轨迹（0..uniqueKeys-1 按轮数重复）
    std::vector<int> makeLoopScanTrace(int uniqueKeys, int rounds)
    {
        std::vector<int> trace;

        // 预分配容量，减少扩容开销
        trace.reserve(static_cast<size_t>(uniqueKeys * rounds));

        // 轮次循环 + 顺序访问
        for (int r = 0; r < rounds; ++r)
        {
            for (int k = 0; k < uniqueKeys; ++k)
            {
                trace.push_back(k);
            }
        }
        
        return trace;
    }

    // 生成热点轨迹（按 hotPercent 概率访问热点集合）
    std::vector<int> makeHotsetTrace(int totalOps, int hotKeys, int coldKeys, int hotPercent)
    {
        std::vector<int> trace;
        trace.reserve(static_cast<size_t>(totalOps)); // 预分配容量
        FastRng rng(0x1029384756ULL);                 // 固定种子，保证可复现

        for (int i = 0; i < totalOps; ++i)
        {
            // 概率控制：是否访问热点集合
            const bool fromHotset = static_cast<int>(rng.next() % 100ULL) < hotPercent;
            if (fromHotset)
            {
                // 热点区间：[0, hotKeys)
                trace.push_back(static_cast<int>(rng.next() % static_cast<uint64_t>(hotKeys)));
            }
            else
            {
                // 冷点区间：[hotKeys, hotKeys + coldKeys)
                trace.push_back(hotKeys + static_cast<int>(rng.next() % static_cast<uint64_t>(coldKeys)));
            }
        }

        return trace;
    }

    // 生成热点漂移轨迹（前后两段热点集合不同）
    std::vector<int> makeShiftedHotsetTrace(int totalOps, int hotKeys)
    {
        std::vector<int> trace;
        trace.reserve(static_cast<size_t>(totalOps)); // 预分配容量
        FastRng rng(0x55667788ULL);                   // 固定种子，保证可复现
        const int half = totalOps / 2;                // 分段点

        // 前半段访问第一组热点：[0, hotKeys)
        for (int i = 0; i < half; ++i)
        {
            trace.push_back(static_cast<int>(rng.next() % static_cast<uint64_t>(hotKeys)));
        }

        // 后半段访问第二组热点：[hotKeys, 2*hotKeys)
        for (int i = half; i < totalOps; ++i)
        {
            trace.push_back(hotKeys + static_cast<int>(rng.next() % static_cast<uint64_t>(hotKeys)));
        }

        return trace;
    }
}