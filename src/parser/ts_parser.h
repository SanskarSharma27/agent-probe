#pragma once

#include <string>
#include <vector>
#include <functional>

#include <tree_sitter/api.h>
#include "parser/ast_node.h"
#include "parser/language_profile.h"

namespace probe {

class TSParserWrapper {
public:
    TSParserWrapper();
    ~TSParserWrapper();

    // Non-copyable
    TSParserWrapper(const TSParserWrapper&) = delete;
    TSParserWrapper& operator=(const TSParserWrapper&) = delete;

    // Set the language grammar (must be called before parsing)
    bool set_language(const TSLanguage* language);

    // Parse a file from disk, returns extracted AST nodes
    std::vector<ASTNode> parse_file(const std::string& file_path, const LanguageProfile& profile);

    // Parse a source string, returns extracted AST nodes
    std::vector<ASTNode> parse_string(const std::string& source,
                                       const std::string& file_path,
                                       const LanguageProfile& profile);

private:
    TSParser* parser_;

    // Extract the text content of a tree-sitter node
    std::string node_text(TSNode node, const std::string& source);

    // Recursively walk to find all call expressions within a subtree
    void collect_calls(TSNode node, const std::string& source,
                       const LanguageProfile& profile,
                       std::vector<std::string>& calls);

    // Extract the function name from a call expression (handles simple + attribute calls)
    std::string extract_call_name(TSNode call_node, const std::string& source,
                                  const LanguageProfile& profile);

    // Extract function definitions from the AST
    void extract_functions(TSNode node, const std::string& source,
                          const std::string& file_path,
                          const LanguageProfile& profile,
                          std::vector<ASTNode>& results,
                          const std::string& parent_class = "");

    // Extract parameter names from a parameters node
    std::vector<std::string> extract_parameters(TSNode params_node, const std::string& source);

    // Collect decorators preceding a definition
    std::vector<std::string> extract_decorators(TSNode decorated_node,
                                                const std::string& source,
                                                const LanguageProfile& profile);

    // Extract import statements
    void extract_imports(TSNode node, const std::string& source,
                        const std::string& file_path,
                        const LanguageProfile& profile,
                        std::vector<ASTNode>& results);

    // Extract class definitions (name, base classes, methods)
    void extract_classes(TSNode node, const std::string& source,
                        const std::string& file_path,
                        const LanguageProfile& profile,
                        std::vector<ASTNode>& results);
};

} // namespace probe
