#pragma once

#include <algorithm>
#include <sstream>
#include <string>

#include "CacheManager.h"

namespace CacheSys
{
    struct StrategyRecommendation
    {
        CacheManager::PolicyType policy;
        std::string policyName;
        std::string reason;
    };

    class StrategySelector
    {
    public:
        // writeRatio/stability are expected in [0, 1]. Values outside this range are clamped.
        static StrategyRecommendation recommend(size_t capacity,
                                                double writeRatio,
                                                double hotspotStability)
        {
            const double w = clamp01(writeRatio);
            const double s = clamp01(hotspotStability);

            std::ostringstream oss;
            oss << "capacity=" << capacity
                << ", writeRatio=" << w
                << ", hotspotStability=" << s << ". ";

            // Frequent writes or highly dynamic hotspot -> LRU tends to adapt faster.
            if (w >= 0.65 || s <= 0.35)
            {
                oss << "Recommend LRU because workload is write-heavy or hotspot changes quickly; "
                    << "recent recency signal is more reliable than long-term frequency.";
                return {CacheManager::PolicyType::LRU, "LRU", oss.str()};
            }

            // Stable hotspots and read-heavy access -> LFU can protect long-term hot keys.
            if (s >= 0.70 && w <= 0.40)
            {
                oss << "Recommend LFU because workload is read-heavy with stable hotspots; "
                    << "frequency signal can preserve durable hot keys effectively.";
                return {CacheManager::PolicyType::LFU, "LFU", oss.str()};
            }

            // Mixed/uncertain mode -> ARC balances recency and frequency adaptively.
            oss << "Recommend ARC because workload is mixed; adaptive balancing between recency "
                << "and frequency usually gives robust hit rate under changing patterns.";
            return {CacheManager::PolicyType::ARC, "ARC", oss.str()};
        }

    private:
        static double clamp01(double v)
        {
            return std::max(0.0, std::min(1.0, v));
        }
    };
}
