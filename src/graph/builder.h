#pragma once

#include <vector>
#include "graph/graph.h"
#include "parser/ast_node.h"

namespace probe {

// Builds a dependency graph from parsed AST nodes.
// Creates graph nodes for functions and classes, then wires up
// edges based on call relationships, imports, and inheritance.
class GraphBuilder {
public:
    Graph build(const std::vector<ASTNode>& ast_nodes);

private:
    // First pass: create graph nodes for all definitions
    void create_nodes(const std::vector<ASTNode>& ast_nodes, Graph& graph);

    // Second pass: wire up call edges by resolving function names
    void create_call_edges(const std::vector<ASTNode>& ast_nodes, Graph& graph);

    // Wire up class inheritance edges
    void create_inheritance_edges(const std::vector<ASTNode>& ast_nodes, Graph& graph);

    // Wire up class-contains-method edges
    void create_containment_edges(const std::vector<ASTNode>& ast_nodes, Graph& graph);

    // Resolve a call name to a graph node ID.
    // Handles: simple names ("foo"), method calls ("obj.method"),
    // and qualified class methods ("ClassName.method")
    int resolve_call(const std::string& call_name, const Graph& graph);
};

} // namespace probe
