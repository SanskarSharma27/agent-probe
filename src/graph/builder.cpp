#include "graph/builder.h"

namespace probe {

Graph GraphBuilder::build(const std::vector<ASTNode>& ast_nodes) {
    Graph graph;
    create_nodes(ast_nodes, graph);
    create_call_edges(ast_nodes, graph);
    create_inheritance_edges(ast_nodes, graph);
    create_containment_edges(ast_nodes, graph);
    return graph;
}

void GraphBuilder::create_nodes(const std::vector<ASTNode>& ast_nodes, Graph& graph) {
    for (const auto& node : ast_nodes) {
        if (node.type == NodeType::FUNCTION_DEF || node.type == NodeType::CLASS_DEF) {
            // Use qualified name for methods: "ClassName.method_name"
            std::string name = node.name;
            if (!node.parent_class.empty()) {
                name = node.parent_class + "." + node.name;
            }

            // Skip if already added (class methods may appear via both class and function extraction)
            if (graph.find_node(name) != -1) continue;

            GraphNode gn;
            gn.name = name;
            gn.file_path = node.file_path;
            gn.line_number = node.start_line;
            gn.node_type = (node.type == NodeType::FUNCTION_DEF) ? "function" : "class";
            graph.add_node(gn);
        }
    }
}

void GraphBuilder::create_call_edges(const std::vector<ASTNode>& ast_nodes, Graph& graph) {
    for (const auto& node : ast_nodes) {
        if (node.type != NodeType::FUNCTION_DEF) continue;

        // Get the caller's graph ID
        std::string caller_name = node.name;
        if (!node.parent_class.empty()) {
            caller_name = node.parent_class + "." + node.name;
        }
        int caller_id = graph.find_node(caller_name);
        if (caller_id == -1) continue;

        for (const auto& call : node.called_functions) {
            int target_id = resolve_call(call, graph);
            if (target_id != -1 && target_id != caller_id) {
                if (!graph.has_edge(caller_id, target_id)) {
                    graph.add_edge(caller_id, target_id, EdgeType::CALLS);
                }
            }
        }
    }
}

void GraphBuilder::create_inheritance_edges(const std::vector<ASTNode>& ast_nodes, Graph& graph) {
    for (const auto& node : ast_nodes) {
        if (node.type != NodeType::CLASS_DEF) continue;

        int class_id = graph.find_node(node.name);
        if (class_id == -1) continue;

        for (const auto& base : node.base_classes) {
            int base_id = graph.find_node(base);
            if (base_id != -1) {
                graph.add_edge(class_id, base_id, EdgeType::INHERITS);
            }
        }
    }
}

void GraphBuilder::create_containment_edges(const std::vector<ASTNode>& ast_nodes, Graph& graph) {
    for (const auto& node : ast_nodes) {
        if (node.type != NodeType::CLASS_DEF) continue;

        int class_id = graph.find_node(node.name);
        if (class_id == -1) continue;

        for (const auto& method : node.methods) {
            std::string qualified = node.name + "." + method;
            int method_id = graph.find_node(qualified);
            if (method_id != -1) {
                graph.add_edge(class_id, method_id, EdgeType::CONTAINS);
            }
        }
    }
}

int GraphBuilder::resolve_call(const std::string& call_name, const Graph& graph) {
    // 1. Try exact match: "foo" or "ClassName.method"
    int id = graph.find_node(call_name);
    if (id != -1) return id;

    // 2. For "obj.method" calls (like "self.process" or "db.query"),
    //    try resolving just the method part against all known functions
    auto dot = call_name.find('.');
    if (dot != std::string::npos) {
        std::string method = call_name.substr(dot + 1);

        // Try as a bare function name
        id = graph.find_node(method);
        if (id != -1) return id;

        // Try against all "Class.method" patterns
        for (int i = 0; i < graph.node_count(); i++) {
            const auto& n = graph.get_node(i);
            auto ndot = n.name.find('.');
            if (ndot != std::string::npos && n.name.substr(ndot + 1) == method) {
                return i;
            }
        }
    }

    return -1;
}

} // namespace probe
