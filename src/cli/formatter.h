#pragma once

#include <string>
#include <vector>
#include "models/finding.h"

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

// Format findings as a colored table (ANSI escape codes)
std::string format_table_color(const std::vector<Finding>& findings, const ScanStats& stats);

// Print a progress line to stderr (overwrites previous line)
void print_progress(const std::string& message, int current, int total);

// Clear the progress line on stderr
void clear_progress();

} // namespace probe
