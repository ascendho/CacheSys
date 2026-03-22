#include "generators.h"

#include <random>

// Eval：评估模块 → Scenario：场景化测试子模块（操作/用例生成实现）
namespace CacheSys::Eval::Scenario
{
    // 生成热点数据场景操作序列（70%热点key访问，30%PUT/70%GET，冷热分离）
    // operations：总操作数 | hotKeys：热点key数量 | coldKeys：冷key数量
    std::vector<Operation> makeHotDataScenarioOps(int operations, int hotKeys, int coldKeys)
    {
        std::vector<Operation> ops;

        // 预分配内存，提升性能
        ops.reserve(static_cast<size_t>(operations)); 

        std::mt19937 rng(20260316);                                   // 固定随机种子，保证轨迹可复现
        std::uniform_int_distribution<int> p01to100(1, 100);          // 1~100均匀分布（用于概率判断）
        std::uniform_int_distribution<int> hotDist(0, hotKeys - 1);   // 热点key范围
        std::uniform_int_distribution<int> coldDist(0, coldKeys - 1); // 冷key范围

        for (int i = 0; i < operations; ++i)
        {
            const bool isPut = p01to100(rng) <= 30;                            // 30%概率为PUT操作，70%为GET
            const bool hitHot = p01to100(rng) <= 70;                           // 70%概率访问热点key，30%访问冷key
            const int key = hitHot ? hotDist(rng) : (hotKeys + coldDist(rng)); // 冷key从hotKeys后开始编号

            // 构造操作：PUT/GET + key + 随机value（key*10+余数，仅用于模拟）
            ops.push_back(Operation{isPut, key, key * 10 + (i % 97)});
        }

        return ops;
    }

    // 生成循环扫描场景操作序列（60%顺序扫描，30%随机读，10%越界读，20%PUT/80%GET）
    // operations：总操作数 | loopSize：单次循环的key范围（0~loopSize-1）
    std::vector<Operation> makeLoopScanScenarioOps(int operations, int loopSize)
    {
        std::vector<Operation> ops;
        ops.reserve(static_cast<size_t>(operations));

        std::mt19937 rng(20260317); // 固定种子，轨迹可复现
        std::uniform_int_distribution<int> p01to100(1, 100);
        std::uniform_int_distribution<int> inRange(0, loopSize - 1);  // 循环内key范围
        std::uniform_int_distribution<int> outRange(0, loopSize - 1); // 越界key偏移

        // 顺序扫描游标（模拟循环遍历）
        int cursor = 0; 
        for (int i = 0; i < operations; ++i)
        {
            // 20%PUT，80%GET
            const bool isPut = p01to100(rng) <= 20; 

            int key = 0;
            const int selector = p01to100(rng);
            if (selector <= 60)
            {
                // 60%：顺序扫描（游标递增，到顶重置）
                key = cursor;
                cursor = (cursor + 1) % loopSize;
            }
            else if (selector <= 90)
            {
                // 30%：循环内随机读
                key = inRange(rng);
            }
            else
            {
                // 10%：循环外越界读（模拟访问冷数据）
                key = loopSize + outRange(rng);
            }

            ops.push_back(Operation{isPut, key, key * 10 + (i % 113)});
        }

        return ops;
    }

    // 生成负载漂移场景操作序列（分5个阶段，各阶段PUT概率/key范围不同，模拟热点动态切换）
    // operations：总操作数（均分5个阶段，每个阶段特征不同）
    std::vector<Operation> makeWorkloadShiftScenarioOps(int operations)
    {
        std::vector<Operation> ops;
        ops.reserve(static_cast<size_t>(operations));

        // 固定种子，轨迹可复现
        std::mt19937 rng(20260318); 
        std::uniform_int_distribution<int> p01to100(1, 100);

        // 总操作数拆分为5个阶段，每阶段长度相等
        const int phaseLen = operations / 5; 

        for (int i = 0; i < operations; ++i)
        {
            // 当前操作所属阶段（0~4）
            const int phase = i / phaseLen; 

            // 各阶段PUT概率不同（模拟读写比动态变化）
            int putProbability = 20;
            switch (phase)
            {
            case 0:
                putProbability = 15;
                break; // 阶段0：低写入（15%PUT）
            case 1:
                putProbability = 30;
                break; // 阶段1：高写入（30%PUT）
            case 2:
                putProbability = 10;
                break; // 阶段2：极低写入（10%PUT）
            case 3:
                putProbability = 25;
                break; // 阶段3：中写入（25%PUT）
            default:
                putProbability = 20;
                break; // 阶段4：默认写入（20%PUT）
            }

            const bool isPut = p01to100(rng) <= putProbability;
            int key = 0;

            // 各阶段key访问特征不同（模拟热点漂移）
            if (phase == 0)
            {
                // 阶段0：访问极小热点集（0~4）
                key = p01to100(rng) % 5; 
            }
            else if (phase == 1)
            {
                // 阶段1：访问大散列集（0~399）
                key = p01to100(rng) % 400; 
            }
            else if (phase == 2)
            {
                // 阶段2：顺序小范围扫描（0~99）
                key = (i - phaseLen * 2) % 100; 
            }
            else if (phase == 3)
            {
                // 阶段3：局部性热点（5个小热点集，每800次切换）
                const int locality = (i / 800) % 5;
                key = locality * 15 + (p01to100(rng) % 15);
            }
            else
            {
                // 阶段4：混合热点（40%旧热点+30%新热点+30%冷数据）
                const int r = p01to100(rng);
                if (r <= 40)
                    // 旧热点（0~4）
                    key = p01to100(rng) % 5; 
                else if (r <= 70)
                    // 新热点（5~49）
                    key = 5 + (p01to100(rng) % 45); 
                else
                    // 冷数据（50~399）
                    key = 50 + (p01to100(rng) % 350); 
            }

            ops.push_back(Operation{isPut, key, key * 10 + (phase * 100 + i % 97)});
        }

        return ops;
    }

    // 生成默认场景化测试用例列表（包含3类典型业务场景，预设容量和操作数）
    std::vector<ScenarioCase> makeDefaultScenarioCases()
    {
        return {
            // 场景1：热点数据访问（容量20，50万操作，20个热点key+5000个冷key）
            ScenarioCase{"测试场景1：热点数据访问测试", 20, makeHotDataScenarioOps(500000, 20, 5000)},
            // 场景2：循环扫描（容量50，20万操作，循环范围500个key）
            ScenarioCase{"测试场景2：循环扫描测试", 50, makeLoopScanScenarioOps(200000, 500)},
            // 场景3：负载剧烈变化（容量30，8万操作，5阶段热点漂移）
            ScenarioCase{"测试场景3：工作负载剧烈变化测试", 30, makeWorkloadShiftScenarioOps(80000)}};
    }
}