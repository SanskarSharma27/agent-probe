#pragma once

#include "analyzers/analyzer.h"

namespace probe {

// Detects functions with high fan-out (many outgoing calls) or high
// betweenness centrality. These are orchestration points — functions
// that coordinate multiple subsystems and are strong candidates for
// agent-based automation.
class FanOutAnalyzer : public Analyzer {
public:
    explicit FanOutAnalyzer(int min_out_degree = 4, double centrality_threshold = 0.15)
        : min_out_degree_(min_out_degree), centrality_threshold_(centrality_threshold) {}

    std::string name() const override { return "fan_out"; }
    std::vector<Finding> analyze(const AnalysisContext& ctx) const override;

private:
    int min_out_degree_;
    double centrality_threshold_;
};

} // namespace probe
