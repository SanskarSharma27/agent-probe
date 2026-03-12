#pragma once

#include <string>
#include <vector>

namespace probe {

// Abstract interface for language-specific parsing rules.
// Each supported language implements this to tell the parser
// what Tree-sitter node types and field names to look for.
class LanguageProfile {
public:
    virtual ~LanguageProfile() = default;

    virtual std::string name() const = 0;
    virtual std::vector<std::string> file_extensions() const = 0;

    // Tree-sitter node type names
    virtual std::string root_node_type() const = 0;     // "module" (Python) or "program" (JS)
    virtual std::string function_def_type() const = 0;
    virtual std::string class_def_type() const = 0;
    virtual std::string call_expression_type() const = 0;
    virtual std::string import_statement_type() const = 0;
    virtual std::string import_from_type() const = 0;
    virtual std::string decorator_type() const = 0;
    virtual std::string attribute_type() const = 0;

    // Optional: additional function-like node types (e.g. arrow_function, method_definition)
    virtual std::string arrow_function_type() const { return ""; }
    virtual std::string method_def_type() const { return ""; }

    // Tree-sitter field names for extracting child nodes
    virtual std::string field_name() const = 0;        // "name" field in function/class def
    virtual std::string field_parameters() const = 0;
    virtual std::string field_body() const = 0;
    virtual std::string field_function() const = 0;     // in call expressions
    virtual std::string field_arguments() const = 0;
    virtual std::string field_object() const = 0;       // in attribute access
    virtual std::string field_attribute() const = 0;
    virtual std::string field_module_name() const = 0;  // in import-from

    // Library names that indicate external API calls
    virtual std::vector<std::string> api_call_indicators() const = 0;
};

} // namespace probe
