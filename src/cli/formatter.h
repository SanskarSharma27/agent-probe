#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "models/finding.h"
#include "graph/graph.h"

namespace probe {

// Stats collected during analysis, passed to formatters
struct ScanStats {
    std::string version;
    std::string path;
    int files_scanned = 0;
    int nodes_parsed = 0;
    int graph_nodes = 0;
    int graph_edges = 0;
    int total_findings = 0;
};

// Format findings as a JSON object
std::string format_json(const std::vector<Finding>& findings, const ScanStats& stats);

// Format findings as an aligned table
std::string format_table(const std::vector<Finding>& findings, const ScanStats& stats);

// Format findings as a brief summary with counts by type
std::string format_summary(const std::vector<Finding>& findings, const ScanStats& stats);

// Format as JSON with full graph structure (nodes + edges + findings) for visualization
std::string format_graph_json(const std::vector<Finding>& findings, const ScanStats& stats,
                               const Graph& graph,
                               const std::unordered_map<int, double>& pagerank);

// Format findings as a colored table (ANSI escape codes)
std::string format_table_color(const std::vector<Finding>& findings, const ScanStats& stats);

// Format findings as an actionable integration plan (Markdown)
std::string format_plan(const std::vector<Finding>& findings, const ScanStats& stats);

// Print a progress line to stderr (overwrites previous line)
void print_progress(const std::string& message, int current, int total);

// Clear the progress line on stderr
void clear_progress();

} // namespace probe
