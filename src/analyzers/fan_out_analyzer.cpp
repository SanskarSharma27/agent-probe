#include "analyzers/fan_out_analyzer.h"
#include "graph/algorithms.h"

#include <algorithm>

namespace probe {

std::vector<Finding> FanOutAnalyzer::analyze(const AnalysisContext& ctx) const {
    std::vector<Finding> findings;
    auto functions = ctx.nodes_of_type(NodeType::FUNCTION_DEF);

    // Find max centrality for normalization
    double max_centrality = 0.0;
    for (const auto& [id, score] : ctx.centrality) {
        max_centrality = std::max(max_centrality, score);
    }

    for (const auto* fn : functions) {
        int gid = ctx.graph_id_for(*fn);
        if (gid == -1) continue;

        int out_deg = algorithms::out_degree(ctx.graph, gid);
        double centrality = 0.0;
        auto it = ctx.centrality.find(gid);
        if (it != ctx.centrality.end()) centrality = it->second;

        // Normalize centrality to 0-1 range
        double norm_centrality = (max_centrality > 0.0)
            ? centrality / max_centrality : 0.0;

        bool high_fan_out = out_deg >= min_out_degree_;
        bool high_centrality = norm_centrality >= centrality_threshold_;

        if (!high_fan_out && !high_centrality) continue;

        Finding f;
        f.file_path = fn->file_path;
        f.line_number = fn->start_line;
        f.function_name = fn->name;
        if (!fn->parent_class.empty()) {
            f.function_name = fn->parent_class + "." + fn->name;
        }
        f.type = FindingType::FAN_OUT;

        // Confidence based on both signals
        double degree_signal = std::min(static_cast<double>(out_deg) / 8.0, 1.0);
        double centrality_signal = norm_centrality;
        f.confidence = std::min(0.3 + 0.4 * degree_signal + 0.3 * centrality_signal, 0.95);

        f.reason = std::to_string(out_deg) + " outgoing calls";
        if (high_centrality) {
            f.reason += ", high centrality";
        }

        f.evidence.push_back("out-degree: " + std::to_string(out_deg));
        f.evidence.push_back("centrality: " + std::to_string(centrality));

        // List the called functions as evidence
        const auto& edges = ctx.graph.get_edges(gid);
        for (const auto& edge : edges) {
            if (edge.type == EdgeType::CALLS) {
                f.evidence.push_back("calls " + ctx.graph.get_node(edge.target).name);
            }
        }

        findings.push_back(std::move(f));
    }

    return findings;
}

} // namespace probe
