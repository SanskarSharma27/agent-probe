#pragma once

#include <string>
#include <vector>

namespace probe {

enum class NodeType {
    FUNCTION_DEF,
    CLASS_DEF,
    FUNCTION_CALL,
    IMPORT
};

struct ASTNode {
    NodeType type;
    std::string name;
    std::string file_path;
    int start_line = 0;
    int end_line = 0;

    // FUNCTION_DEF: class this function belongs to (empty if top-level)
    std::string parent_class;

    // FUNCTION_DEF: parameter names
    std::vector<std::string> parameters;

    // FUNCTION_DEF: names of functions/methods called within the body
    std::vector<std::string> called_functions;

    // FUNCTION_DEF / CLASS_DEF: decorator names (e.g., "app.route", "retry")
    std::vector<std::string> decorators;

    // CLASS_DEF: base class names
    std::vector<std::string> base_classes;

    // CLASS_DEF: method names defined in the class
    std::vector<std::string> methods;

    // IMPORT: module being imported ("from X" or "import X")
    std::string module;

    // IMPORT: specific names imported (empty for "import X" style)
    std::vector<std::string> imported_names;

    // FUNCTION_CALL: object the method is called on (e.g., "requests" in "requests.get()")
    std::string object;

    // FUNCTION_CALL: number of arguments passed
    int arg_count = 0;
};

} // namespace probe
