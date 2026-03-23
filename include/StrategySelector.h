#pragma once

#include <algorithm>
#include <sstream>
#include <string>

#include "CacheManager.h"

namespace CacheSys
{
    /**
     * @struct StrategyRecommendation
     * @brief 策略推荐结果结构体
     * 
     * 包含推荐的策略枚举、易读的名称以及推荐该策略的详细逻辑依据。
     */
    struct StrategyRecommendation
    {
        CacheManager::PolicyType policy; ///< 推荐的策略类型枚举 (LRU/LFU/ARC)
        std::string policyName;          ///< 策略的名称字符串
        std::string reason;              ///< 推荐原因及参数分析描述
    };

    /**
     * @class StrategySelector
     * @brief 缓存策略智能选择器
     * 
     * 根据工作负载特征（Workload Characteristics）通过启发式算法推荐最佳策略。
     * 主要考量因素包括：写操作频率、热点数据的稳定性等。
     */
    class StrategySelector
    {
    public:
        /**
         * @brief 根据业务指标推荐最合适的缓存策略
         * 
         * @param capacity 预期的缓存容量。
         * @param writeRatio 写请求占比 (0.0 ~ 1.0)。
         *        - 高写占比通常意味着数据更新频繁。
         * @param hotspotStability 热点数据的稳定性 (0.0 ~ 1.0)。
         *        - 1.0 表示热点 key 非常固定（适合频率统计）。
         *        - 0.0 表示热点 key 随机切换（适合时间局部性）。
         * 
         * @return StrategyRecommendation 包含推荐结论和逻辑解释的对象。
         */
        static StrategyRecommendation recommend(size_t capacity,
                                                double writeRatio,
                                                double hotspotStability)
        {
            // 将输入参数限制在 [0, 1] 区间内
            const double w = clamp01(writeRatio);
            const double s = clamp01(hotspotStability);

            std::ostringstream oss;
            oss << "capacity=" << capacity
                << ", writeRatio=" << w
                << ", hotspotStability=" << s << ". ";

            /**
             * 场景 1：写密集型 或 热点极不稳定
             * 逻辑：如果写占比 > 65% 或者热点变化非常快，
             * 频率统计（LFU）会失效，且容易产生频率污染。
             * 此时“最近访问（LRU）”提供的时效性信号最可靠。
             */
            if (w >= 0.65 || s <= 0.35)
            {
                oss << "Recommend LRU because workload is write-heavy or hotspot changes quickly; "
                    << "recent recency signal is more reliable than long-term frequency.";
                return {CacheManager::PolicyType::LRU, "LRU", oss.str()};
            }

            /**
             * 场景 2：读密集型 且 热点非常稳定
             * 逻辑：如果数据主要是读，且热点 key 长期不变，
             * LFU（最不经常使用）能完美保留那些高频访问的“镇馆之宝” Key，
             * 不会被偶发性的批量扫描请求（如数据库全表扫描）冲刷掉。
             */
            if (s >= 0.70 && w <= 0.40)
            {
                oss << "Recommend LFU because workload is read-heavy with stable hotspots; "
                    << "frequency signal can preserve durable hot keys effectively.";
                return {CacheManager::PolicyType::LFU, "LFU", oss.str()};
            }

            /**
             * 场景 3：混合型 或 动态负载
             * 逻辑：如果不属于上述极端情况，推荐使用 ARC（自适应替换缓存）。
             * ARC 会根据实时命中率动态调整 LRU 和 LFU 的比例，
             * 在访问模式发生演变时具有最强的鲁棒性。
             */
            oss << "Recommend ARC because workload is mixed; adaptive balancing between recency "
                << "and frequency usually gives robust hit rate under changing patterns.";
            return {CacheManager::PolicyType::ARC, "ARC", oss.str()};
        }

    private:
        /**
         * @brief 将数值限制在 [0, 1] 范围内的辅助函数
         */
        static double clamp01(double v)
        {
            return std::max(0.0, std::min(1.0, v));
        }
    };
}