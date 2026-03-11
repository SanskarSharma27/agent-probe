#include "scoring/scorer.h"

#include <algorithm>
#include <cmath>

namespace probe {

void Scorer::set_weight(FindingType type, double weight) {
    type_weights_[static_cast<int>(type)] = weight;
}

double Scorer::get_weight(FindingType type) const {
    auto it = type_weights_.find(static_cast<int>(type));
    if (it != type_weights_.end()) return it->second;

    // Default weights per type
    switch (type) {
        case FindingType::API_CALL:      return 1.0;
        case FindingType::FAN_OUT:       return 0.9;
        case FindingType::RETRY_PATTERN: return 1.1;  // retry patterns are high value
        case FindingType::CRUD_CLUSTER:  return 0.85;
    }
    return 1.0;
}

double Scorer::pagerank_boost(const Finding& f, const AnalysisContext& ctx) const {
    int gid = ctx.graph.find_node(f.function_name);
    if (gid == -1) return 0.0;

    auto it = ctx.pagerank_scores.find(gid);
    if (it == ctx.pagerank_scores.end()) return 0.0;

    // Normalize: if this node's PageRank is above average, boost confidence
    double avg = 1.0 / std::max(ctx.graph.node_count(), 1);
    double ratio = it->second / avg;

    // Clamp boost to [0, 0.1] range
    if (ratio > 1.5) return 0.1;
    if (ratio > 1.0) return 0.05;
    return 0.0;
}

std::vector<Finding> Scorer::score(const std::vector<Finding>& raw_findings,
                                    const AnalysisContext& ctx) const {
    std::vector<Finding> scored;
    scored.reserve(raw_findings.size());

    for (auto f : raw_findings) {
        double weight = get_weight(f.type);
        double boost = pagerank_boost(f, ctx);

        // Apply weight and boost, clamp to [0.0, 1.0]
        f.confidence = std::min(std::max(f.confidence * weight + boost, 0.0), 1.0);

        // Round to 2 decimal places for clean output
        f.confidence = std::round(f.confidence * 100.0) / 100.0;

        scored.push_back(std::move(f));
    }

    // Sort by confidence descending
    std::sort(scored.begin(), scored.end(),
              [](const Finding& a, const Finding& b) {
                  return a.confidence > b.confidence;
              });

    return scored;
}

} // namespace probe
