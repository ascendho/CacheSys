#include "generators.h"

#include <random>

/**
 * @brief 缓存评估负载生成器实现
 */
namespace CacheSys::Eval::Scenario
{
    /**
     * @brief 模拟“热点数据”访问场景（符合 80/20 法则）
     * 
     * 逻辑：模拟一小部分数据（Hot）被频繁访问，而大部分数据（Cold）很少被访问的情况。
     * - 约 70% 的请求集中在少数热点 Key 上。
     * - 约 30% 的请求为写入 (Put) 操作。
     * 
     * @param operations 总操作指令数
     * @param hotKeys 热点键的数量（频繁访问区间）
     * @param coldKeys 冷数据键的数量（罕见访问区间）
     */
    std::vector<Operation> makeHotDataScenarioOps(int operations, int hotKeys, int coldKeys)
    {
        std::vector<Operation> ops;
        ops.reserve(static_cast<size_t>(operations)); 

        // 使用固定种子确保测试结果的可复现性（Deterministic Benchmarking）
        std::mt19937 rng(20260316);                                   
        std::uniform_int_distribution<int> p01to100(1, 100);          // 用于概率判定
        std::uniform_int_distribution<int> hotDist(0, hotKeys - 1);   // 热点区间分布
        std::uniform_int_distribution<int> coldDist(0, coldKeys - 1); // 冷数据区间分布

        for (int i = 0; i < operations; ++i)
        {
            // 判定操作类型：30% 概率为写入，70% 为读取
            const bool isPut = p01to100(rng) <= 30;                            
            // 判定访问区域：70% 概率命中热点区域
            const bool hitHot = p01to100(rng) <= 70;                           
            
            // 计算 Key：如果是热点则从前段取，否则从后段冷数据区取
            const int key = hitHot ? hotDist(rng) : (hotKeys + coldDist(rng)); 

            // 生成操作并存入，Value 采用简单线性变换模拟变化的数据
            ops.push_back(Operation{isPut, key, key * 10 + (i % 97)});
        }

        return ops;
    }

    /**
     * @brief 模拟“循环扫描”场景（Sequence Scan / Loop）
     * 
     * 逻辑：模拟数据库全表扫描或循环遍历大数据集的行为。
     * - 核心在于 cursor 的递增，产生 1, 2, 3... N, 1, 2... 的访问序列。
     * - 当 loopSize 大于缓存容量时，LRU 算法会导致严重的“缓存污染”。
     * 
     * @param operations 总操作指令数
     * @param loopSize 循环数据集的大小
     */
    std::vector<Operation> makeLoopScanScenarioOps(int operations, int loopSize)
    {
        std::vector<Operation> ops;
        ops.reserve(static_cast<size_t>(operations));

        std::mt19937 rng(20260317); 
        std::uniform_int_distribution<int> p01to100(1, 100);
        std::uniform_int_distribution<int> inRange(0, loopSize - 1);  
        std::uniform_int_distribution<int> outRange(0, loopSize - 1); 

        int cursor = 0; // 记录顺序扫描的当前位置
        for (int i = 0; i < operations; ++i)
        {
            const bool isPut = p01to100(rng) <= 20; // 20% 写入概率

            int key = 0;
            const int selector = p01to100(rng);
            if (selector <= 60)
            {
                // 60% 概率：执行严格的顺序扫描（循环递增）
                key = cursor;
                cursor = (cursor + 1) % loopSize;
            }
            else if (selector <= 90)
            {
                // 30% 概率：在扫描范围内随机访问（局部性抖动）
                key = inRange(rng);
            }
            else
            {
                // 10% 概率：访问范围外的“噪声”数据
                key = loopSize + outRange(rng);
            }

            ops.push_back(Operation{isPut, key, key * 10 + (i % 113)});
        }

        return ops;
    }

    /**
     * @brief 模拟“工作负载剧烈切换”场景（Workload Shift）
     * 
     * 逻辑：将测试分为 5 个阶段，每个阶段的访问特征和写占比都完全不同。
     * 这是一个深度评估算法“自适应能力”的高级测试。
     * 
     * @param operations 总操作指令数
     */
    std::vector<Operation> makeWorkloadShiftScenarioOps(int operations)
    {
        std::vector<Operation> ops;
        ops.reserve(static_cast<size_t>(operations));

        std::mt19937 rng(20260318); 
        std::uniform_int_distribution<int> p01to100(1, 100);

        const int phaseLen = operations / 5; // 平均分为五个阶段

        for (int i = 0; i < operations; ++i)
        {
            const int phase = i / phaseLen; // 当前处于第几个阶段

            // 动态调整每个阶段的写入概率
            int putProbability = 20;
            switch (phase)
            {
            case 0: putProbability = 15; break; // 阶段0：读多
            case 1: putProbability = 30; break; // 阶段1：写多
            case 2: putProbability = 10; break; // 阶段2：极高频读取
            case 3: putProbability = 25; break; // 阶段3：混合
            default: putProbability = 20; break;
            }

            const bool isPut = p01to100(rng) <= putProbability;
            int key = 0;

            // 根据不同阶段模拟完全不同的数据分布
            if (phase == 0)
            {
                // 阶段 0：极小范围高频热点（只有 5 个 Key，考察快速热启动）
                key = p01to100(rng) % 5; 
            }
            else if (phase == 1)
            {
                // 阶段 1：大范围稀疏随机访问（考察抗污染性）
                key = p01to100(rng) % 400; 
            }
            else if (phase == 2)
            {
                // 阶段 2：滑动窗口顺序访问（考察对局部性的捕捉）
                key = (i - phaseLen * 2) % 100; 
            }
            else if (phase == 3)
            {
                // 阶段 3：块状局部性（聚集在几个特定的“块”内）
                const int locality = (i / 800) % 5;
                key = locality * 15 + (p01to100(rng) % 15);
            }
            else
            {
                // 阶段 4：复合型访问（混合了极热数据和长尾数据）
                const int r = p01to100(rng);
                if (r <= 40)
                    key = p01to100(rng) % 5; // 重访阶段0的热点
                else if (r <= 70)
                    key = 5 + (p01to100(rng) % 45); // 中频数据
                else
                    key = 50 + (p01to100(rng) % 350); // 低频数据
            }

            ops.push_back(Operation{isPut, key, key * 10 + (phase * 100 + i % 97)});
        }

        return ops;
    }

    /**
     * @brief 构建默认的综合测试用例集合
     * 
     * 将上述各种模拟场景打包成标准用例，分配不同的缓存容量以观察算法表现。
     * - 热点测试：容量给 20（远小于数据量），看命中率。
     * - 循环扫描：容量给 50（数据量 500），看 LRU 表现是否极其糟糕。
     * - 负载切换：考察自适应能力。
     */
    std::vector<ScenarioCase> makeDefaultScenarioCases()
    {
        return {
            ScenarioCase{"测试场景1：热点数据访问测试", 20, makeHotDataScenarioOps(500000, 20, 5000)},
            ScenarioCase{"测试场景2：循环扫描测试", 50, makeLoopScanScenarioOps(200000, 500)},
            ScenarioCase{"测试场景3：工作负载剧烈变化测试", 30, makeWorkloadShiftScenarioOps(80000)}};
    }
}