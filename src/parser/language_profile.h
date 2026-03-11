#pragma once

#include <string>
#include <vector>

namespace probe {

// Abstract interface for language-specific parsing rules.
// Each supported language implements this to tell the parser
// what Tree-sitter node types to look for.
class LanguageProfile {
public:
    virtual ~LanguageProfile() = default;

    virtual std::string name() const = 0;
    virtual std::vector<std::string> file_extensions() const = 0;

    // Tree-sitter node type names for this language
    virtual std::string function_def_type() const = 0;
    virtual std::string class_def_type() const = 0;
    virtual std::string call_expression_type() const = 0;
    virtual std::string import_type() const = 0;

    // Patterns that indicate external API calls (library names)
    virtual std::vector<std::string> api_call_indicators() const = 0;
};

} // namespace probe
