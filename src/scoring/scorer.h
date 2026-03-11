#pragma once

#include <vector>
#include <unordered_map>
#include "models/finding.h"
#include "analyzers/analysis_context.h"

namespace probe {

// Combines raw analyzer findings with graph-level metrics into
// normalized 0.0-1.0 confidence scores. Applies configurable
// type-based weights and graph-signal boosts.
class Scorer {
public:
    // Re-score findings using the analysis context for graph boosts
    std::vector<Finding> score(const std::vector<Finding>& raw_findings,
                               const AnalysisContext& ctx) const;

    // Set a custom weight multiplier for a finding type (default 1.0)
    void set_weight(FindingType type, double weight);

private:
    std::unordered_map<int, double> type_weights_;

    double get_weight(FindingType type) const;

    // Boost based on graph position (PageRank)
    double pagerank_boost(const Finding& f, const AnalysisContext& ctx) const;
};

} // namespace probe
