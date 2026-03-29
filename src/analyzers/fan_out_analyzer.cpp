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

    // Compute average PageRank for floor filtering
    double avg_pagerank = 0.0;
    if (!ctx.pagerank_scores.empty()) {
        for (const auto& [id, score] : ctx.pagerank_scores) {
            avg_pagerank += score;
        }
        avg_pagerank /= ctx.pagerank_scores.size();
    }

    for (const auto* fn : functions) {
        int gid = ctx.graph_id_for(*fn);
        if (gid == -1) continue;

        // Count only CALLS edges for out-degree (not CONTAINS, IMPORTS, etc.)
        int call_out_deg = 0;
        const auto& edges = ctx.graph.get_edges(gid);
        for (const auto& edge : edges) {
            if (edge.type == EdgeType::CALLS) call_out_deg++;
        }

        double centrality = 0.0;
        auto it = ctx.centrality.find(gid);
        if (it != ctx.centrality.end()) centrality = it->second;

        // Normalize centrality to 0-1 range
        double norm_centrality = (max_centrality > 0.0)
            ? centrality / max_centrality : 0.0;

        // Require high fan-out as the primary signal
        // Centrality is a secondary signal that boosts confidence, not a standalone trigger
        bool high_fan_out = call_out_deg >= min_out_degree_;
        if (!high_fan_out) continue;

        // In large graphs, filter out functions with very low PageRank —
        // if nobody important calls you, you're probably not an orchestrator.
        // Skip this filter for small graphs (< 20 nodes) where PageRank isn't meaningful.
        if (ctx.graph.node_count() >= 20) {
            double pr = 0.0;
            auto pr_it = ctx.pagerank_scores.find(gid);
            if (pr_it != ctx.pagerank_scores.end()) pr = pr_it->second;
            if (pr < avg_pagerank * 0.5) continue;
        }

        bool high_centrality = norm_centrality >= centrality_threshold_;

        Finding f;
        f.file_path = fn->file_path;
        f.line_number = fn->start_line;
        f.function_name = fn->name;
        if (!fn->parent_class.empty()) {
            f.function_name = fn->parent_class + "." + fn->name;
        }
        f.type = FindingType::FAN_OUT;

        // Confidence: fan-out degree is the base, centrality boosts it
        double degree_signal = std::min(static_cast<double>(call_out_deg) / 8.0, 1.0);
        double centrality_signal = norm_centrality;
        f.confidence = std::min(0.25 + 0.45 * degree_signal + 0.25 * centrality_signal, 0.95);

        f.reason = std::to_string(call_out_deg) + " outgoing calls";
        if (high_centrality) {
            f.reason += ", high centrality";
        }

        f.evidence.push_back("out-degree: " + std::to_string(call_out_deg));
        f.evidence.push_back("centrality: " + std::to_string(centrality));

        // List the called functions as evidence
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
