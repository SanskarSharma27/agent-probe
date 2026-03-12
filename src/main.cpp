#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <cstdlib>
#include <array>
#include <regex>

#include <CLI/CLI.hpp>
#include <tree_sitter/api.h>

#include "parser/ts_parser.h"
#include "parser/python_profile.h"
#include "parser/javascript_profile.h"
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
extern "C" const TSLanguage *tree_sitter_javascript();

namespace fs = std::filesystem;
using namespace probe;

const std::string VERSION = "0.1.0";

// Directories to skip during recursive scanning
static const std::unordered_set<std::string> SKIP_DIRS = {
    ".git", ".svn", ".hg",
    ".venv", "venv", "env", ".env",
    "node_modules",
    "__pycache__", ".mypy_cache", ".pytest_cache", ".tox", ".nox",
    "build", "dist", "out", "_build",
    ".next", ".nuxt",
    "site-packages",
    "vendor",
    "coverage", ".coverage",
    ".eggs", "*.egg-info",
    ".idea", ".vscode",
    "target",  // Rust/Java
    "cmake-build-debug", "cmake-build-release",
};

// Maps file extension → (profile, TSLanguage)
struct LangEntry {
    const LanguageProfile* profile;
    const TSLanguage* ts_lang;
};

// Recursively collect all files matching any supported extension
static std::vector<std::string> collect_files(
    const std::string& root,
    const std::unordered_map<std::string, LangEntry>& lang_map,
    const std::vector<std::string>& extra_excludes = {}) {

    // Merge default skip set with user-provided excludes
    std::unordered_set<std::string> skip = SKIP_DIRS;
    for (const auto& ex : extra_excludes) {
        skip.insert(ex);
    }

    std::vector<std::string> files;
    std::error_code ec;

    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); ) {

        if (ec) {
            std::cerr << "Warning: " << ec.message() << "\n";
            ec.clear();
            ++it;
            continue;
        }

        const auto& entry = *it;

        // Skip excluded directories
        if (entry.is_directory()) {
            std::string dirname = entry.path().filename().string();
            if (skip.count(dirname) || (!dirname.empty() && dirname[0] == '.')) {
                it.disable_recursion_pending();
                ++it;
                continue;
            }
        }

        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            if (lang_map.count(ext)) {
                files.push_back(entry.path().string());
            }
        }

        ++it;
    }

    return files;
}

// Check if a string looks like a GitHub URL
static bool is_github_url(const std::string& s) {
    // Match https://github.com/owner/repo or git@github.com:owner/repo
    static const std::regex gh_regex(
        R"(^(https?://github\.com/[\w.\-]+/[\w.\-]+|git@github\.com:[\w.\-]+/[\w.\-]+)(\.git)?(/.*)?$)",
        std::regex::icase);
    return std::regex_match(s, gh_regex);
}

// Clone a git repo to a temporary directory, return the path
static std::string clone_repo(const std::string& url) {
    // Create temp dir
    std::string tmp_base = fs::temp_directory_path().string();
    std::string tmp_dir = tmp_base + "/agent-probe-clone-" +
        std::to_string(std::hash<std::string>{}(url) % 1000000);

    // Remove if exists from a previous run
    std::error_code ec;
    fs::remove_all(tmp_dir, ec);

    std::string cmd = "git clone --depth 1 --quiet \"" + url + "\" \"" + tmp_dir + "\" 2>&1";

    std::array<char, 256> buffer;
    std::string output;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "Error: failed to run git clone\n";
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        output += buffer.data();
    }
    int status = pclose(pipe);

    if (status != 0) {
        std::cerr << "Error: git clone failed: " << output << "\n";
        return "";
    }

    return tmp_dir;
}

// Clean up a cloned temp directory
static void cleanup_clone(const std::string& dir) {
    std::error_code ec;
    fs::remove_all(dir, ec);
    // Silently ignore errors — Windows may hold .git locks briefly
}

int main(int argc, char* argv[]) {
    CLI::App app{"agent-probe — static analysis for agent integration points"};
    app.set_version_flag("--version", VERSION);

    std::string path = ".";
    std::string format = "table";
    double min_confidence = 0.0;
    bool no_color = false;
    std::vector<std::string> excludes;

    app.add_option("-p,--path", path, "Path to repository or GitHub URL to scan");
    app.add_option("-f,--format", format, "Output format: table, json, summary, graph")
        ->check(CLI::IsMember({"table", "json", "summary", "graph"}));
    app.add_option("-c,--min-confidence", min_confidence, "Minimum confidence threshold (0.0-1.0)")
        ->check(CLI::Range(0.0, 1.0));
    app.add_flag("--no-color", no_color, "Disable colored output");
    app.add_option("-e,--exclude", excludes, "Additional directory names to exclude (repeatable)");

    CLI11_PARSE(app, argc, argv);

    // Handle GitHub URL: clone to temp dir
    std::string cloned_dir;
    if (is_github_url(path)) {
        if (!no_color && format != "json" && format != "graph") {
            std::cerr << "Cloning " << path << " ...\n";
        }
        cloned_dir = clone_repo(path);
        if (cloned_dir.empty()) {
            std::cerr << "Error: failed to clone repository: " << path << "\n";
            return 2;
        }
        path = cloned_dir;
    }

    // Validate path exists
    if (!fs::exists(path)) {
        std::cerr << "Error: path does not exist: " << path << "\n";
        return 2;
    }

    // Register language profiles
    PythonProfile python_profile;
    JavaScriptProfile js_profile;

    std::unordered_map<std::string, LangEntry> lang_map;
    for (const auto& ext : python_profile.file_extensions()) {
        lang_map[ext] = {&python_profile, tree_sitter_python()};
    }
    for (const auto& ext : js_profile.file_extensions()) {
        lang_map[ext] = {&js_profile, tree_sitter_javascript()};
    }

    // Collect source files
    std::vector<std::string> files;
    if (fs::is_regular_file(path)) {
        files.push_back(path);
    } else {
        files = collect_files(path, lang_map, excludes);
    }

    if (files.empty()) {
        std::cerr << "No supported files found in: " << path << "\n";
        if (!cloned_dir.empty()) cleanup_clone(cloned_dir);
        return 2;
    }

    // Parse all files (switch parser language per file)
    TSParserWrapper parser;
    const TSLanguage* current_lang = nullptr;

    std::vector<ASTNode> all_nodes;
    int files_parsed = 0;
    int total_files = static_cast<int>(files.size());
    bool show_progress = !no_color && format != "json" && format != "graph";

    for (const auto& file : files) {
        files_parsed++;
        if (show_progress) {
            std::string short_name = fs::path(file).filename().string();
            print_progress("Parsing " + short_name, files_parsed, total_files);
        }

        std::string ext = fs::path(file).extension().string();
        auto it = lang_map.find(ext);
        if (it == lang_map.end()) continue;

        // Switch language if needed
        if (it->second.ts_lang != current_lang) {
            parser.set_language(it->second.ts_lang);
            current_lang = it->second.ts_lang;
        }

        auto nodes = parser.parse_file(file, *it->second.profile);
        all_nodes.insert(all_nodes.end(), nodes.begin(), nodes.end());
    }

    if (show_progress) print_progress("Building graph", 0, 0);

    // Build call graph
    GraphBuilder builder;
    Graph graph = builder.build(all_nodes);

    if (show_progress) print_progress("Computing metrics", 0, 0);

    // Compute graph metrics
    auto centrality = algorithms::betweenness_centrality(graph);
    auto pagerank = algorithms::pagerank(graph);

    if (show_progress) clear_progress();

    // Build analysis context (use python_profile as default for analyzers)
    // The analyzers check api_call_indicators which are language-specific,
    // so we pick the profile that has the most files. For now, use a combined approach.
    // Since AnalysisContext needs a single profile reference, we'll use whichever
    // language had files. For mixed repos, Python takes priority.
    const LanguageProfile* primary_profile = &python_profile;
    {
        int py_count = 0, js_count = 0;
        for (const auto& file : files) {
            std::string ext = fs::path(file).extension().string();
            if (ext == ".py") py_count++;
            else js_count++;
        }
        if (js_count > py_count) primary_profile = &js_profile;
    }

    AnalysisContext ctx{graph, all_nodes, *primary_profile, centrality, pagerank, {}};

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
    if (format == "graph") {
        std::cout << format_graph_json(filtered, stats, graph, pagerank) << "\n";
    } else if (format == "json") {
        std::cout << format_json(filtered, stats) << "\n";
    } else if (format == "summary") {
        std::cout << format_summary(filtered, stats);
    } else if (no_color) {
        std::cout << format_table(filtered, stats);
    } else {
        std::cout << format_table_color(filtered, stats);
    }

    // Clean up cloned repo if we created one
    if (!cloned_dir.empty()) cleanup_clone(cloned_dir);

    return filtered.empty() ? 0 : 1;
}
