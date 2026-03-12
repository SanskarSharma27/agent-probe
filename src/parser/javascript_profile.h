#pragma once

#include "parser/language_profile.h"

namespace probe {

class JavaScriptProfile : public LanguageProfile {
public:
    std::string name() const override { return "JavaScript"; }
    std::vector<std::string> file_extensions() const override { return {".js", ".jsx", ".ts", ".tsx", ".mjs"}; }

    // Tree-sitter JavaScript grammar node types
    std::string root_node_type() const override { return "program"; }
    std::string function_def_type() const override { return "function_declaration"; }
    std::string class_def_type() const override { return "class_declaration"; }
    std::string call_expression_type() const override { return "call_expression"; }
    std::string import_statement_type() const override { return "import_statement"; }
    std::string import_from_type() const override { return ""; }  // JS uses import_statement for all
    std::string decorator_type() const override { return "decorator"; }
    std::string attribute_type() const override { return "member_expression"; }

    // JS-specific: arrow functions and class method definitions
    std::string arrow_function_type() const override { return "arrow_function"; }
    std::string method_def_type() const override { return "method_definition"; }

    // Tree-sitter JavaScript field names
    std::string field_name() const override { return "name"; }
    std::string field_parameters() const override { return "parameters"; }
    std::string field_body() const override { return "body"; }
    std::string field_function() const override { return "function"; }
    std::string field_arguments() const override { return "arguments"; }
    std::string field_object() const override { return "object"; }
    std::string field_attribute() const override { return "property"; }  // JS uses "property" not "attribute"
    std::string field_module_name() const override { return "source"; }  // import ... from "source"

    // JS/Node.js libraries that indicate external API calls
    std::vector<std::string> api_call_indicators() const override {
        return {
            "fetch", "axios", "got", "node-fetch",
            "superagent", "request", "http", "https",
            "grpc", "redis", "mongoose", "sequelize",
            "knex", "prisma", "pg", "mysql",
            "aws-sdk", "firebase",
        };
    }
};

} // namespace probe
