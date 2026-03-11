#pragma once

#include "parser/language_profile.h"

namespace probe {

class PythonProfile : public LanguageProfile {
public:
    std::string name() const override { return "Python"; }
    std::vector<std::string> file_extensions() const override { return {".py"}; }

    // Tree-sitter Python grammar node types
    std::string function_def_type() const override { return "function_definition"; }
    std::string class_def_type() const override { return "class_definition"; }
    std::string call_expression_type() const override { return "call"; }
    std::string import_statement_type() const override { return "import_statement"; }
    std::string import_from_type() const override { return "import_from_statement"; }
    std::string decorator_type() const override { return "decorator"; }
    std::string attribute_type() const override { return "attribute"; }

    // Tree-sitter Python field names
    std::string field_name() const override { return "name"; }
    std::string field_parameters() const override { return "parameters"; }
    std::string field_body() const override { return "body"; }
    std::string field_function() const override { return "function"; }
    std::string field_arguments() const override { return "arguments"; }
    std::string field_object() const override { return "object"; }
    std::string field_attribute() const override { return "attribute"; }
    std::string field_module_name() const override { return "module_name"; }

    // Python libraries that indicate external HTTP/API calls
    std::vector<std::string> api_call_indicators() const override {
        return {
            "requests", "httpx", "aiohttp", "urllib",
            "grpc", "boto3", "redis", "pymongo",
            "sqlalchemy", "psycopg2", "mysql"
        };
    }
};

} // namespace probe
