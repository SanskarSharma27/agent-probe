#pragma once

#include <string>
#include <vector>

namespace probe {

enum class NodeType {
    FUNCTION_DEF,
    CLASS_DEF,
    FUNCTION_CALL,
    IMPORT,
    METHOD_CALL
};

struct ASTNode {
    NodeType type;
    std::string name;
    std::string file_path;
    int start_line;
    int end_line;
    std::string parent_class;              // empty if top-level
    std::vector<std::string> parameters;
    std::vector<std::string> called_functions;
    std::vector<std::string> decorators;
};

} // namespace probe
