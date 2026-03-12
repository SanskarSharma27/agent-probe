#include <gtest/gtest.h>
#include <algorithm>

#include "parser/ts_parser.h"
#include "parser/javascript_profile.h"

extern "C" const TSLanguage *tree_sitter_javascript();

using namespace probe;

class JSParserFixture : public ::testing::Test {
protected:
    TSParserWrapper parser;
    JavaScriptProfile profile;

    void SetUp() override {
        parser.set_language(tree_sitter_javascript());
    }

    std::vector<ASTNode> parse(const std::string& code) {
        return parser.parse_string(code, "test.js", profile);
    }

    const ASTNode* find_node(const std::vector<ASTNode>& nodes,
                              const std::string& name, NodeType type) {
        for (const auto& n : nodes) {
            if (n.name == name && n.type == type) return &n;
        }
        return nullptr;
    }
};

// ═══════════════════════════════════════════════════════════════════
//  Function Declaration Tests
// ═══════════════════════════════════════════════════════════════════

TEST_F(JSParserFixture, ExtractsNamedFunction) {
    auto nodes = parse("function hello(name) { console.log(name); }");

    auto* fn = find_node(nodes, "hello", NodeType::FUNCTION_DEF);
    ASSERT_NE(fn, nullptr);
    EXPECT_EQ(fn->name, "hello");
    EXPECT_EQ(fn->parameters.size(), 1u);
    EXPECT_EQ(fn->parameters[0], "name");
}

TEST_F(JSParserFixture, ExtractsMultipleFunctions) {
    auto nodes = parse(R"(
        function foo() {}
        function bar() {}
        function baz() {}
    )");

    int func_count = 0;
    for (const auto& n : nodes) {
        if (n.type == NodeType::FUNCTION_DEF) func_count++;
    }
    EXPECT_EQ(func_count, 3);
}

TEST_F(JSParserFixture, ExtractsCallExpressions) {
    auto nodes = parse(R"(
        function handler() {
            console.log('start');
            fetch('/api/data');
            processResult();
        }
    )");

    auto* fn = find_node(nodes, "handler", NodeType::FUNCTION_DEF);
    ASSERT_NE(fn, nullptr);
    EXPECT_GE(fn->called_functions.size(), 2u);

    auto has_call = [&](const std::string& name) {
        return std::find(fn->called_functions.begin(),
                         fn->called_functions.end(), name) != fn->called_functions.end();
    };
    EXPECT_TRUE(has_call("fetch"));
    EXPECT_TRUE(has_call("processResult"));
}

// ═══════════════════════════════════════════════════════════════════
//  Arrow Function Tests
// ═══════════════════════════════════════════════════════════════════

TEST_F(JSParserFixture, ExtractsArrowFunction) {
    auto nodes = parse(R"(
        const greet = (name) => {
            console.log('hello ' + name);
        };
    )");

    auto* fn = find_node(nodes, "greet", NodeType::FUNCTION_DEF);
    ASSERT_NE(fn, nullptr);
    EXPECT_EQ(fn->name, "greet");
}

TEST_F(JSParserFixture, ArrowFunctionCapturesCalls) {
    auto nodes = parse(R"(
        const fetchData = async (url) => {
            const resp = await axios.get(url);
            return resp.data;
        };
    )");

    auto* fn = find_node(nodes, "fetchData", NodeType::FUNCTION_DEF);
    ASSERT_NE(fn, nullptr);

    auto has_call = [&](const std::string& name) {
        return std::find(fn->called_functions.begin(),
                         fn->called_functions.end(), name) != fn->called_functions.end();
    };
    EXPECT_TRUE(has_call("axios.get"));
}

// ═══════════════════════════════════════════════════════════════════
//  Class Tests
// ═══════════════════════════════════════════════════════════════════

TEST_F(JSParserFixture, ExtractsClassDefinition) {
    auto nodes = parse(R"(
        class Animal {
            constructor(name) {
                this.name = name;
            }
            speak() {
                console.log(this.name + ' speaks');
            }
        }
    )");

    auto* cls = find_node(nodes, "Animal", NodeType::CLASS_DEF);
    ASSERT_NE(cls, nullptr);
    EXPECT_GE(cls->methods.size(), 2u);

    bool has_constructor = std::find(cls->methods.begin(), cls->methods.end(), "constructor")
                           != cls->methods.end();
    bool has_speak = std::find(cls->methods.begin(), cls->methods.end(), "speak")
                     != cls->methods.end();
    EXPECT_TRUE(has_constructor);
    EXPECT_TRUE(has_speak);
}

TEST_F(JSParserFixture, ExtractsClassWithExtends) {
    auto nodes = parse(R"(
        class Dog extends Animal {
            bark() { console.log('woof'); }
        }
    )");

    auto* cls = find_node(nodes, "Dog", NodeType::CLASS_DEF);
    ASSERT_NE(cls, nullptr);
    EXPECT_EQ(cls->base_classes.size(), 1u);
    EXPECT_EQ(cls->base_classes[0], "Animal");
}

TEST_F(JSParserFixture, ClassMethodsHaveParentClass) {
    auto nodes = parse(R"(
        class Service {
            async getData() {
                return await fetch('/api');
            }
        }
    )");

    auto* method = find_node(nodes, "getData", NodeType::FUNCTION_DEF);
    ASSERT_NE(method, nullptr);
    EXPECT_EQ(method->parent_class, "Service");
}

// ═══════════════════════════════════════════════════════════════════
//  Import Tests
// ═══════════════════════════════════════════════════════════════════

TEST_F(JSParserFixture, ExtractsDefaultImport) {
    auto nodes = parse("import express from 'express';");

    auto* imp = find_node(nodes, "express", NodeType::IMPORT);
    ASSERT_NE(imp, nullptr);
    EXPECT_EQ(imp->module, "express");
}

TEST_F(JSParserFixture, ExtractsNamedImports) {
    auto nodes = parse("import { useState, useEffect } from 'react';");

    auto* imp = find_node(nodes, "react", NodeType::IMPORT);
    ASSERT_NE(imp, nullptr);
    EXPECT_EQ(imp->module, "react");
    EXPECT_GE(imp->imported_names.size(), 2u);
}

TEST_F(JSParserFixture, ExtractsMemberExpression) {
    auto nodes = parse(R"(
        function makeRequest() {
            axios.get('/api');
            axios.post('/api', data);
        }
    )");

    auto* fn = find_node(nodes, "makeRequest", NodeType::FUNCTION_DEF);
    ASSERT_NE(fn, nullptr);

    auto has_call = [&](const std::string& name) {
        return std::find(fn->called_functions.begin(),
                         fn->called_functions.end(), name) != fn->called_functions.end();
    };
    EXPECT_TRUE(has_call("axios.get"));
    EXPECT_TRUE(has_call("axios.post"));
}

// ═══════════════════════════════════════════════════════════════════
//  Express Fixture Integration Test
// ═══════════════════════════════════════════════════════════════════

TEST_F(JSParserFixture, ExpressFixtureFile) {
    auto nodes = parser.parse_file(
        "fixtures/javascript/express_app.js", profile);

    // Should find functions, classes, and imports
    if (nodes.empty()) {
        // File not found (path relative to build dir) — skip
        GTEST_SKIP() << "Fixture file not accessible from test CWD";
    }

    int functions = 0, classes = 0, imports = 0;
    for (const auto& n : nodes) {
        if (n.type == NodeType::FUNCTION_DEF) functions++;
        else if (n.type == NodeType::CLASS_DEF) classes++;
        else if (n.type == NodeType::IMPORT) imports++;
    }

    EXPECT_GT(functions, 5);
    EXPECT_GE(classes, 2);
    EXPECT_GE(imports, 1);
}
