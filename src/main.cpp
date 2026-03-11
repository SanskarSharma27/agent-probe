#include <iostream>
#include <string>
#include <cstring>

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <tree_sitter/api.h>

extern "C" const TSLanguage *tree_sitter_python();

const std::string VERSION = "0.1.0";

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

    // Quick tree-sitter verification
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_python());

    std::string source = "def hello(): pass\n";
    TSTree* tree = ts_parser_parse_string(parser, nullptr, source.c_str(), source.size());
    TSNode root = ts_tree_root_node(tree);

    nlohmann::json info = {
        {"version", VERSION},
        {"path", path},
        {"format", format},
        {"min_confidence", min_confidence},
        {"tree_sitter_ok", !ts_node_is_null(root)},
        {"root_children", ts_node_child_count(root)}
    };

    if (format == "json") {
        std::cout << info.dump(2) << "\n";
    } else {
        std::cout << "agent-probe v" << VERSION << "\n";
        std::cout << "  target:     " << path << "\n";
        std::cout << "  format:     " << format << "\n";
        std::cout << "  threshold:  " << min_confidence << "\n";
        std::cout << "  tree-sitter: ok\n";
    }

    ts_tree_delete(tree);
    ts_parser_delete(parser);

    return 0;
}
