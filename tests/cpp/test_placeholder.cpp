#include <gtest/gtest.h>
#include <tree_sitter/api.h>
#include "parser/ts_parser.h"
#include "parser/python_profile.h"

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

// ─── Parser wrapper tests ──────────────────────────────────────

TEST(ParserTest, ExtractsSimpleFunction) {
    probe::TSParserWrapper parser;
    parser.set_language(tree_sitter_python());
    probe::PythonProfile profile;

    std::string code = "def hello(name):\n    print(name)\n";
    auto nodes = parser.parse_string(code, "test.py", profile);

    ASSERT_EQ(nodes.size(), 1u);
    EXPECT_EQ(nodes[0].type, probe::NodeType::FUNCTION_DEF);
    EXPECT_EQ(nodes[0].name, "hello");
    EXPECT_EQ(nodes[0].start_line, 1);
    EXPECT_EQ(nodes[0].parameters.size(), 1u);
    EXPECT_EQ(nodes[0].parameters[0], "name");
    EXPECT_EQ(nodes[0].called_functions.size(), 1u);
    EXPECT_EQ(nodes[0].called_functions[0], "print");
}

TEST(ParserTest, ExtractsMethodCalls) {
    probe::TSParserWrapper parser;
    parser.set_language(tree_sitter_python());
    probe::PythonProfile profile;

    std::string code =
        "def fetch(url):\n"
        "    response = requests.get(url)\n"
        "    return response.json()\n";
    auto nodes = parser.parse_string(code, "test.py", profile);

    ASSERT_EQ(nodes.size(), 1u);
    auto& fn = nodes[0];
    EXPECT_EQ(fn.name, "fetch");
    ASSERT_GE(fn.called_functions.size(), 2u);
    EXPECT_EQ(fn.called_functions[0], "requests.get");
    EXPECT_EQ(fn.called_functions[1], "response.json");
}

TEST(ParserTest, ExtractsMultipleFunctions) {
    probe::TSParserWrapper parser;
    parser.set_language(tree_sitter_python());
    probe::PythonProfile profile;

    std::string code =
        "def foo():\n    pass\n\n"
        "def bar(x, y):\n    return x + y\n";
    auto nodes = parser.parse_string(code, "test.py", profile);

    ASSERT_EQ(nodes.size(), 2u);
    EXPECT_EQ(nodes[0].name, "foo");
    EXPECT_EQ(nodes[0].parameters.size(), 0u);
    EXPECT_EQ(nodes[1].name, "bar");
    EXPECT_EQ(nodes[1].parameters.size(), 2u);
}
