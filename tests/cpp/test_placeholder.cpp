#include <gtest/gtest.h>
#include <tree_sitter/api.h>

extern "C" const TSLanguage *tree_sitter_python();

TEST(TreeSitterTest, ParserInitializes) {
    TSParser* parser = ts_parser_new();
    ASSERT_NE(parser, nullptr);

    bool ok = ts_parser_set_language(parser, tree_sitter_python());
    EXPECT_TRUE(ok);

    ts_parser_delete(parser);
}

TEST(TreeSitterTest, ParsesSimplePython) {
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_python());

    std::string code = "x = 42\n";
    TSTree* tree = ts_parser_parse_string(parser, nullptr, code.c_str(), code.size());
    TSNode root = ts_tree_root_node(tree);

    EXPECT_FALSE(ts_node_is_null(root));
    EXPECT_STREQ(ts_node_type(root), "module");
    EXPECT_GE(ts_node_child_count(root), 1u);

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

TEST(TreeSitterTest, DetectsFunctionDefinition) {
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_python());

    std::string code = "def greet(name):\n    print(name)\n";
    TSTree* tree = ts_parser_parse_string(parser, nullptr, code.c_str(), code.size());
    TSNode root = ts_tree_root_node(tree);
    TSNode first_child = ts_node_child(root, 0);

    EXPECT_STREQ(ts_node_type(first_child), "function_definition");

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}
