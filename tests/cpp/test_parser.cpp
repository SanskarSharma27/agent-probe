#include <gtest/gtest.h>
#include <algorithm>
#include "parser/ts_parser.h"
#include "parser/python_profile.h"

extern "C" const TSLanguage *tree_sitter_python();

class ParserFixture : public ::testing::Test {
protected:
    probe::TSParserWrapper parser;
    probe::PythonProfile profile;

    void SetUp() override {
        parser.set_language(tree_sitter_python());
    }

    // Helpers to find nodes by type and name
    std::vector<probe::ASTNode> nodes_of_type(const std::vector<probe::ASTNode>& nodes,
                                               probe::NodeType type) {
        std::vector<probe::ASTNode> result;
        for (auto& n : nodes) {
            if (n.type == type) result.push_back(n);
        }
        return result;
    }

    const probe::ASTNode* find_by_name(const std::vector<probe::ASTNode>& nodes,
                                        const std::string& name) {
        for (auto& n : nodes) {
            if (n.name == name) return &n;
        }
        return nullptr;
    }

    bool has_call(const probe::ASTNode& fn, const std::string& call_name) {
        return std::find(fn.called_functions.begin(), fn.called_functions.end(), call_name)
               != fn.called_functions.end();
    }
};

// ─── Import extraction ─────────────────────────────────────────

TEST_F(ParserFixture, ExtractsSimpleImport) {
    std::string code = "import os\nimport sys\n";
    auto nodes = parser.parse_string(code, "test.py", profile);
    auto imports = nodes_of_type(nodes, probe::NodeType::IMPORT);

    ASSERT_EQ(imports.size(), 2u);
    EXPECT_EQ(imports[0].module, "os");
    EXPECT_EQ(imports[1].module, "sys");
}

TEST_F(ParserFixture, ExtractsFromImport) {
    std::string code = "from flask import Flask, request, jsonify\n";
    auto nodes = parser.parse_string(code, "test.py", profile);
    auto imports = nodes_of_type(nodes, probe::NodeType::IMPORT);

    ASSERT_EQ(imports.size(), 1u);
    EXPECT_EQ(imports[0].module, "flask");
    ASSERT_EQ(imports[0].imported_names.size(), 3u);
    EXPECT_EQ(imports[0].imported_names[0], "Flask");
    EXPECT_EQ(imports[0].imported_names[1], "request");
    EXPECT_EQ(imports[0].imported_names[2], "jsonify");
}

TEST_F(ParserFixture, ExtractsFromImportSubmodule) {
    std::string code = "from datetime import datetime\n";
    auto nodes = parser.parse_string(code, "test.py", profile);
    auto imports = nodes_of_type(nodes, probe::NodeType::IMPORT);

    ASSERT_EQ(imports.size(), 1u);
    EXPECT_EQ(imports[0].module, "datetime");
}

// ─── Class extraction ──────────────────────────────────────────

TEST_F(ParserFixture, ExtractsClassDefinition) {
    std::string code =
        "class Animal:\n"
        "    def __init__(self, name):\n"
        "        self.name = name\n"
        "\n"
        "    def speak(self):\n"
        "        pass\n";
    auto nodes = parser.parse_string(code, "test.py", profile);
    auto classes = nodes_of_type(nodes, probe::NodeType::CLASS_DEF);

    ASSERT_EQ(classes.size(), 1u);
    EXPECT_EQ(classes[0].name, "Animal");
    ASSERT_EQ(classes[0].methods.size(), 2u);
    EXPECT_EQ(classes[0].methods[0], "__init__");
    EXPECT_EQ(classes[0].methods[1], "speak");
}

TEST_F(ParserFixture, ExtractsClassWithInheritance) {
    std::string code =
        "class Dog(Animal):\n"
        "    def speak(self):\n"
        "        return 'woof'\n";
    auto nodes = parser.parse_string(code, "test.py", profile);
    auto classes = nodes_of_type(nodes, probe::NodeType::CLASS_DEF);

    ASSERT_EQ(classes.size(), 1u);
    EXPECT_EQ(classes[0].name, "Dog");
    ASSERT_GE(classes[0].base_classes.size(), 1u);
    EXPECT_EQ(classes[0].base_classes[0], "Animal");
}

TEST_F(ParserFixture, ClassMethodsHaveParentClass) {
    std::string code =
        "class MyService:\n"
        "    def handle(self, data):\n"
        "        self.process(data)\n";
    auto nodes = parser.parse_string(code, "test.py", profile);
    auto funcs = nodes_of_type(nodes, probe::NodeType::FUNCTION_DEF);

    ASSERT_EQ(funcs.size(), 1u);
    EXPECT_EQ(funcs[0].name, "handle");
    EXPECT_EQ(funcs[0].parent_class, "MyService");
}

// ─── Decorator extraction ──────────────────────────────────────

TEST_F(ParserFixture, ExtractsDecoratedFunction) {
    std::string code =
        "@app.route('/users', methods=['GET'])\n"
        "def list_users():\n"
        "    return get_all()\n";
    auto nodes = parser.parse_string(code, "test.py", profile);
    auto funcs = nodes_of_type(nodes, probe::NodeType::FUNCTION_DEF);

    ASSERT_EQ(funcs.size(), 1u);
    EXPECT_EQ(funcs[0].name, "list_users");
    ASSERT_EQ(funcs[0].decorators.size(), 1u);
    EXPECT_EQ(funcs[0].decorators[0], "app.route");
}

TEST_F(ParserFixture, ExtractsMultipleDecorators) {
    std::string code =
        "@login_required\n"
        "@cache(timeout=60)\n"
        "def dashboard():\n"
        "    pass\n";
    auto nodes = parser.parse_string(code, "test.py", profile);
    auto funcs = nodes_of_type(nodes, probe::NodeType::FUNCTION_DEF);

    ASSERT_EQ(funcs.size(), 1u);
    ASSERT_EQ(funcs[0].decorators.size(), 2u);
    EXPECT_EQ(funcs[0].decorators[0], "login_required");
    EXPECT_EQ(funcs[0].decorators[1], "cache");
}

// ─── Call extraction ───────────────────────────────────────────

TEST_F(ParserFixture, ExtractsNestedCalls) {
    std::string code =
        "def process():\n"
        "    data = fetch_data()\n"
        "    result = transform(data)\n"
        "    save(result)\n"
        "    notify_slack(result)\n";
    auto nodes = parser.parse_string(code, "test.py", profile);
    auto funcs = nodes_of_type(nodes, probe::NodeType::FUNCTION_DEF);

    ASSERT_EQ(funcs.size(), 1u);
    EXPECT_EQ(funcs[0].called_functions.size(), 4u);
    EXPECT_TRUE(has_call(funcs[0], "fetch_data"));
    EXPECT_TRUE(has_call(funcs[0], "transform"));
    EXPECT_TRUE(has_call(funcs[0], "save"));
    EXPECT_TRUE(has_call(funcs[0], "notify_slack"));
}

TEST_F(ParserFixture, ExtractsChainedMethodCalls) {
    std::string code =
        "def query():\n"
        "    result = db.session.query(User).filter(active=True).all()\n";
    auto nodes = parser.parse_string(code, "test.py", profile);
    auto funcs = nodes_of_type(nodes, probe::NodeType::FUNCTION_DEF);

    ASSERT_EQ(funcs.size(), 1u);
    // Should capture the chain
    EXPECT_GE(funcs[0].called_functions.size(), 1u);
}

// ─── Fixture file tests ───────────────────────────────────────

TEST_F(ParserFixture, FlaskAppFixture) {
    auto nodes = parser.parse_file("fixtures/python/flask_app.py", profile);
    ASSERT_GT(nodes.size(), 0u);

    auto imports = nodes_of_type(nodes, probe::NodeType::IMPORT);
    auto classes = nodes_of_type(nodes, probe::NodeType::CLASS_DEF);
    auto funcs = nodes_of_type(nodes, probe::NodeType::FUNCTION_DEF);

    // Should find imports: os, time, flask, datetime, requests
    EXPECT_GE(imports.size(), 4u);

    // Should find classes: DatabaseClient, UserService
    ASSERT_EQ(classes.size(), 2u);
    EXPECT_NE(find_by_name(nodes, "DatabaseClient"), nullptr);
    EXPECT_NE(find_by_name(nodes, "UserService"), nullptr);

    // Should find top-level functions + class methods
    EXPECT_GE(funcs.size(), 5u);

    // Check decorated route handlers exist
    auto* list_fn = find_by_name(nodes, "list_users");
    ASSERT_NE(list_fn, nullptr);
    EXPECT_EQ(list_fn->decorators.size(), 1u);
    EXPECT_EQ(list_fn->decorators[0], "app.route");

    // Check sync_with_retry has calls to requests.post and time.sleep
    auto* sync_fn = find_by_name(nodes, "sync_with_retry");
    ASSERT_NE(sync_fn, nullptr);
    EXPECT_TRUE(has_call(*sync_fn, "requests.post"));
    EXPECT_TRUE(has_call(*sync_fn, "time.sleep"));
}

TEST_F(ParserFixture, DataPipelineFixture) {
    auto nodes = parser.parse_file("fixtures/python/data_pipeline.py", profile);
    ASSERT_GT(nodes.size(), 0u);

    auto funcs = nodes_of_type(nodes, probe::NodeType::FUNCTION_DEF);

    // Should find: load_data, transform, clean_text, validate_record, save_results, run_pipeline
    EXPECT_GE(funcs.size(), 6u);

    // run_pipeline should call load_data, transform, save_results
    auto* pipeline = find_by_name(nodes, "run_pipeline");
    ASSERT_NE(pipeline, nullptr);
    EXPECT_TRUE(has_call(*pipeline, "load_data"));
    EXPECT_TRUE(has_call(*pipeline, "transform"));
    EXPECT_TRUE(has_call(*pipeline, "save_results"));

    // transform should call clean_text and validate_record
    auto* transform_fn = find_by_name(nodes, "transform");
    ASSERT_NE(transform_fn, nullptr);
    EXPECT_TRUE(has_call(*transform_fn, "clean_text"));
    EXPECT_TRUE(has_call(*transform_fn, "validate_record"));
}
