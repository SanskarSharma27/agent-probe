#include <iostream>
#include <string>
#include <cstring>

#include <tree_sitter/api.h>

// Tree-sitter Python language declaration
extern "C" const TSLanguage *tree_sitter_python();

const std::string VERSION = "0.1.0";

void print_ast(TSNode node, const std::string& source, int depth = 0) {
    for (int i = 0; i < depth; i++) std::cout << "  ";

    const char* type = ts_node_type(node);
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);

    std::cout << type;
    if (ts_node_child_count(node) == 0) {
        std::cout << " \"" << source.substr(start, end - start) << "\"";
    }

    TSPoint start_point = ts_node_start_point(node);
    std::cout << " [" << start_point.row + 1 << ":" << start_point.column << "]\n";

    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        print_ast(ts_node_child(node, i), source, depth + 1);
    }
}

int main(int argc, char* argv[]) {
    std::cout << "agent-probe v" << VERSION << "\n\n";

    // Verify tree-sitter works with a sample Python snippet
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_python());

    std::string source =
        "import requests\n"
        "\n"
        "def fetch_data(url):\n"
        "    response = requests.get(url)\n"
        "    return response.json()\n";

    TSTree* tree = ts_parser_parse_string(
        parser, nullptr, source.c_str(), source.size()
    );

    TSNode root = ts_tree_root_node(tree);

    std::cout << "Parsed Python AST (" << ts_node_child_count(root) << " top-level nodes):\n";
    std::cout << std::string(50, '-') << "\n";
    print_ast(root, source);

    ts_tree_delete(tree);
    ts_parser_delete(parser);

    return 0;
}
