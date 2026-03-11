#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

#include <CLI/CLI.hpp>
#include <tree_sitter/api.h>

#include "parser/ts_parser.h"
#include "parser/python_profile.h"
#include "graph/builder.h"
#include "graph/algorithms.h"
#include "analyzers/analysis_context.h"
#include "analyzers/api_call_analyzer.h"
#include "analyzers/fan_out_analyzer.h"
#include "analyzers/retry_analyzer.h"
#include "analyzers/crud_analyzer.h"
#include "scoring/scorer.h"
#include "models/finding.h"
#include "cli/formatter.h"

extern "C" const TSLanguage *tree_sitter_python();

namespace fs = std::filesystem;
using namespace probe;

const std::string VERSION = "0.1.0";

// Recursively collect all files matching the profile's extensions
static std::vector<std::string> collect_files(const std::string& root,
                                               const std::vector<std::string>& extensions) {
    std::vector<std::string> files;

    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(root, ec)) {
        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        for (const auto& target_ext : extensions) {
            if (ext == target_ext) {
                files.push_back(entry.path().string());
                break;
            }
        }
    }

    if (ec) {
        std::cerr << "Warning: error scanning directory: " << ec.message() << "\n";
    }

    return files;
}

int main(int argc, char* argv[]) {
    CLI::App app{"agent-probe — static analysis for agent integration points"};
    app.set_version_flag("--version", VERSION);

    std::string path = ".";
    std::string format = "table";
    double min_confidence = 0.0;
    bool no_color = false;

    app.add_option("-p,--path", path, "Path to repository to scan");
    app.add_option("-f,--format", format, "Output format: table, json, summary")
        ->check(CLI::IsMember({"table", "json", "summary"}));
    app.add_option("-c,--min-confidence", min_confidence, "Minimum confidence threshold (0.0-1.0)")
        ->check(CLI::Range(0.0, 1.0));
    app.add_flag("--no-color", no_color, "Disable colored output");

    CLI11_PARSE(app, argc, argv);

    // Validate path exists
    if (!fs::exists(path)) {
        std::cerr << "Error: path does not exist: " << path << "\n";
        return 2;
    }

    // Set up language profile
    PythonProfile profile;

    // Collect source files
    std::vector<std::string> files;
    if (fs::is_regular_file(path)) {
        files.push_back(path);
    } else {
        files = collect_files(path, profile.file_extensions());
    }

    if (files.empty()) {
        std::cerr << "No " << profile.name() << " files found in: " << path << "\n";
        return 2;
    }

    // Parse all files
    TSParserWrapper parser;
    parser.set_language(tree_sitter_python());

    std::vector<ASTNode> all_nodes;
    int files_parsed = 0;
    int total_files = static_cast<int>(files.size());

    for (const auto& file : files) {
        files_parsed++;
        if (!no_color && format != "json") {
            std::string short_name = fs::path(file).filename().string();
            print_progress("Parsing " + short_name, files_parsed, total_files);
        }
        auto nodes = parser.parse_file(file, profile);
        all_nodes.insert(all_nodes.end(), nodes.begin(), nodes.end());
    }

    if (!no_color && format != "json") {
        print_progress("Building graph", 0, 0);
    }

    // Build call graph
    GraphBuilder builder;
    Graph graph = builder.build(all_nodes);

    if (!no_color && format != "json") {
        print_progress("Computing metrics", 0, 0);
    }

    // Compute graph metrics
    auto centrality = algorithms::betweenness_centrality(graph);
    auto pagerank = algorithms::pagerank(graph);

    if (!no_color && format != "json") {
        clear_progress();
    }

    // Build analysis context
    AnalysisContext ctx{graph, all_nodes, profile, centrality, pagerank, {}};

    // Run all analyzers
    std::vector<Finding> all_findings;

    ApiCallAnalyzer api_analyzer;
    FanOutAnalyzer fan_out_analyzer;
    RetryAnalyzer retry_analyzer;
    CrudAnalyzer crud_analyzer;

    auto append = [&](const std::vector<Finding>& f) {
        all_findings.insert(all_findings.end(), f.begin(), f.end());
    };

    append(api_analyzer.analyze(ctx));
    append(fan_out_analyzer.analyze(ctx));
    append(retry_analyzer.analyze(ctx));
    append(crud_analyzer.analyze(ctx));

    // Score and sort
    Scorer scorer;
    auto scored = scorer.score(all_findings, ctx);

    // Filter by minimum confidence
    std::vector<Finding> filtered;
    for (const auto& f : scored) {
        if (f.confidence >= min_confidence) {
            filtered.push_back(f);
        }
    }

    // Build stats for formatters
    ScanStats stats;
    stats.version = VERSION;
    stats.path = path;
    stats.files_scanned = files_parsed;
    stats.nodes_parsed = static_cast<int>(all_nodes.size());
    stats.graph_nodes = graph.node_count();
    stats.graph_edges = graph.edge_count();
    stats.total_findings = static_cast<int>(scored.size());

    // Output results
    // Exit codes: 0 = no findings, 1 = findings found, 2 = error
    if (format == "json") {
        std::cout << format_json(filtered, stats) << "\n";
    } else if (format == "summary") {
        std::cout << format_summary(filtered, stats);
    } else if (no_color) {
        std::cout << format_table(filtered, stats);
    } else {
        std::cout << format_table_color(filtered, stats);
    }

    return filtered.empty() ? 0 : 1;
}
