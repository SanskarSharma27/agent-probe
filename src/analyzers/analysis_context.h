#pragma once

#include <vector>
#include <string>
#include <unordered_map>

#include "parser/ast_node.h"
#include "parser/language_profile.h"
#include "graph/graph.h"

namespace probe {

// Bundles all data an analyzer might need for its analysis.
// Built once after parsing + graph construction, shared across all analyzers.
struct AnalysisContext {
    const Graph& graph;
    const std::vector<ASTNode>& ast_nodes;
    const LanguageProfile& profile;

    // Pre-computed graph metrics
    std::unordered_map<int, double> centrality;
    std::unordered_map<int, double> pagerank_scores;

    // Source code by file path (for analyzers that need raw AST access)
    std::unordered_map<std::string, std::string> file_sources;

    // Helper: find all AST nodes of a given type
    std::vector<const ASTNode*> nodes_of_type(NodeType type) const {
        std::vector<const ASTNode*> result;
        for (const auto& n : ast_nodes) {
            if (n.type == type) result.push_back(&n);
        }
        return result;
    }

    // Helper: find AST node by name
    const ASTNode* find_ast_node(const std::string& name) const {
        for (const auto& n : ast_nodes) {
            if (n.name == name) return &n;
        }
        return nullptr;
    }

    // Helper: get graph node ID for an AST function node
    int graph_id_for(const ASTNode& node) const {
        std::string name = node.name;
        if (!node.parent_class.empty()) {
            name = node.parent_class + "." + node.name;
        }
        return graph.find_node(name);
    }
};

} // namespace probe
