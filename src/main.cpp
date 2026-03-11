#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
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

    app.add_option("-p,--path", path, "Path to repository to scan");
    app.add_option("-f,--format", format, "Output format: table, json, summary")
        ->check(CLI::IsMember({"table", "json", "summary"}));
    app.add_option("-c,--min-confidence", min_confidence, "Minimum confidence threshold (0.0-1.0)")
        ->check(CLI::Range(0.0, 1.0));

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

    for (const auto& file : files) {
        auto nodes = parser.parse_file(file, profile);
        all_nodes.insert(all_nodes.end(), nodes.begin(), nodes.end());
        files_parsed++;
    }

    // Build call graph
    GraphBuilder builder;
    Graph graph = builder.build(all_nodes);

    // Compute graph metrics
    auto centrality = algorithms::betweenness_centrality(graph);
    auto pagerank = algorithms::pagerank(graph);

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

    // Output results
    if (format == "json") {
        nlohmann::json output;
        output["version"] = VERSION;
        output["path"] = path;
        output["files_scanned"] = files_parsed;
        output["nodes_parsed"] = all_nodes.size();
        output["graph_nodes"] = graph.node_count();
        output["graph_edges"] = graph.edge_count();
        output["total_findings"] = scored.size();
        output["shown_findings"] = filtered.size();

        nlohmann::json findings_json = nlohmann::json::array();
        for (const auto& f : filtered) {
            nlohmann::json fj;
            fj["file"] = f.file_path;
            fj["line"] = f.line_number;
            fj["function"] = f.function_name;
            fj["type"] = finding_type_str(f.type);
            fj["confidence"] = f.confidence;
            fj["reason"] = f.reason;
            fj["evidence"] = f.evidence;
            findings_json.push_back(fj);
        }
        output["findings"] = findings_json;
        std::cout << output.dump(2) << "\n";

    } else if (format == "summary") {
        std::cout << "agent-probe v" << VERSION << "\n";
        std::cout << "Scanned " << files_parsed << " files, "
                  << all_nodes.size() << " AST nodes\n";
        std::cout << "Graph: " << graph.node_count() << " nodes, "
                  << graph.edge_count() << " edges\n";
        std::cout << "\nFindings: " << filtered.size();
        if (min_confidence > 0.0) {
            std::cout << " (threshold >= " << min_confidence << ")";
        }
        std::cout << "\n";

        // Count by type
        int api = 0, fan = 0, retry = 0, crud = 0;
        for (const auto& f : filtered) {
            switch (f.type) {
                case FindingType::API_CALL: api++; break;
                case FindingType::FAN_OUT: fan++; break;
                case FindingType::RETRY_PATTERN: retry++; break;
                case FindingType::CRUD_CLUSTER: crud++; break;
            }
        }
        if (api)   std::cout << "  API calls:      " << api << "\n";
        if (fan)   std::cout << "  Fan-out:        " << fan << "\n";
        if (retry) std::cout << "  Retry patterns: " << retry << "\n";
        if (crud)  std::cout << "  CRUD clusters:  " << crud << "\n";

    } else {
        // Table format
        std::cout << "agent-probe v" << VERSION << " — " << files_parsed
                  << " files, " << graph.node_count() << " graph nodes\n\n";

        if (filtered.empty()) {
            std::cout << "No findings";
            if (min_confidence > 0.0) {
                std::cout << " above threshold " << min_confidence;
            }
            std::cout << ".\n";
        } else {
            // Header
            printf("%-6s  %-12s  %-8s  %-30s  %s\n",
                   "CONF", "TYPE", "LINE", "FUNCTION", "FILE");
            printf("%-6s  %-12s  %-8s  %-30s  %s\n",
                   "------", "------------", "--------",
                   "------------------------------",
                   "--------------------");

            for (const auto& f : filtered) {
                printf("%-6.2f  %-12s  %-8d  %-30s  %s\n",
                       f.confidence,
                       finding_type_str(f.type).c_str(),
                       f.line_number,
                       f.function_name.c_str(),
                       f.file_path.c_str());
            }

            std::cout << "\n" << filtered.size() << " finding(s)\n";
        }
    }

    return filtered.empty() ? 0 : 1;
}
