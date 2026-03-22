#include "generators.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

// Eval：评估模块 → Trace：轨迹生成/解析工具实现
namespace CacheSys::Eval::Trace
{
    // 匿名命名空间：仅当前编译单元可见的快速随机数生成器（轻量、无锁）
    namespace
    {
        struct FastRng
        {
            // 构造函数：初始化随机数种子
            explicit FastRng(uint64_t seed) : state(seed) {}

            // 生成下一个随机数（线性同余算法，速度快，适合模拟场景）
            uint64_t next()
            {
                state = state * 6364136223846793005ULL + 1ULL;
                return state;
            }

            // 随机数状态（种子+累计值）
            uint64_t state;
        };
    }

    // 解析CSV格式的容量字符串为整数数组（如"64,128,256" → [64,128,256]）
    std::vector<int> parseCapacities(const std::string &csv)
    {
        std::vector<int> capacities;

        // 字符串流拆分CSV
        std::stringstream ss(csv);
        std::string item;

        // 按逗号分割字符串，转换为整数存入数组
        while (std::getline(ss, item, ','))
        {
            if (!item.empty())
            {
                capacities.push_back(std::stoi(item));
            }
        }

        return capacities;
    }

    // 从文件加载缓存访问轨迹（文件每行一个整数key）
    std::vector<int> loadTraceFromFile(const std::string &path)
    {
        std::ifstream ifs(path);

        // 文件打开失败抛出异常，明确错误原因
        if (!ifs)
        {
            throw std::runtime_error("Failed to open trace file: " + path);
        }

        std::vector<int> trace;
        int key = 0;

        // 逐行读取key，存入轨迹数组
        while (ifs >> key)
        {
            trace.push_back(key);
        }

        return trace;
    }

    // 生成循环扫描型轨迹（模拟顺序遍历全量数据场景）
    // uniqueKeys：唯一key总数 | rounds：循环遍历轮数
    std::vector<int> makeLoopScanTrace(int uniqueKeys, int rounds)
    {
        std::vector<int> trace;

        // 预分配内存，提升性能
        trace.reserve(static_cast<size_t>(uniqueKeys * rounds));

        // 多轮顺序遍历所有唯一key
        for (int r = 0; r < rounds; ++r)
        {
            for (int k = 0; k < uniqueKeys; ++k)
            {
                trace.push_back(k);
            }
        }
        
        return trace;
    }

    // 生成热点集型轨迹（模拟冷热数据分离场景，如爆款商品访问）
    // totalOps：总访问次数 | hotKeys：热点key数 | coldKeys：冷key数 | hotPercent：热点访问占比(0~100)
    std::vector<int> makeHotsetTrace(int totalOps, int hotKeys, int coldKeys, int hotPercent)
    {
        std::vector<int> trace;
        trace.reserve(static_cast<size_t>(totalOps)); // 预分配内存
        FastRng rng(0x1029384756ULL);                 // 固定种子，保证轨迹可复现

        for (int i = 0; i < totalOps; ++i)
        {
            // 随机判断是否访问热点集（按hotPercent控制占比）
            const bool fromHotset = static_cast<int>(rng.next() % 100ULL) < hotPercent;
            if (fromHotset)
            {
                // 访问热点key（0 ~ hotKeys-1）
                trace.push_back(static_cast<int>(rng.next() % static_cast<uint64_t>(hotKeys)));
            }
            else
            {
                // 访问冷key（hotKeys ~ hotKeys+coldKeys-1）
                trace.push_back(hotKeys + static_cast<int>(rng.next() % static_cast<uint64_t>(coldKeys)));
            }
        }

        return trace;
    }

    // 生成热点漂移型轨迹（模拟热点动态切换场景，如直播带货换款）
    // totalOps：总访问次数 | hotKeys：每轮热点key数量（前半/后半段热点不同）
    std::vector<int> makeShiftedHotsetTrace(int totalOps, int hotKeys)
    {
        std::vector<int> trace;
        trace.reserve(static_cast<size_t>(totalOps)); // 预分配内存
        FastRng rng(0x55667788ULL);                   // 固定种子，轨迹可复现
        const int half = totalOps / 2;                // 总访问数拆分为前后两段

        // 前半段：访问第一批热点（0 ~ hotKeys-1）
        for (int i = 0; i < half; ++i)
        {
            trace.push_back(static_cast<int>(rng.next() % static_cast<uint64_t>(hotKeys)));
        }

        // 后半段：访问第二批热点（hotKeys ~ 2*hotKeys-1）→ 模拟热点漂移
        for (int i = half; i < totalOps; ++i)
        {
            trace.push_back(hotKeys + static_cast<int>(rng.next() % static_cast<uint64_t>(hotKeys)));
        }

        return trace;
    }
}