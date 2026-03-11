#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace probe {

enum class EdgeType {
    CALLS,        // function A calls function B
    IMPORTS,      // module A imports module B
    INHERITS,     // class A inherits class B
    CONTAINS      // class A contains method B
};

struct Edge {
    int target;
    EdgeType type;
    double weight;
};

struct GraphNode {
    int id;
    std::string name;
    std::string file_path;
    int line_number;
    std::string node_type;   // "function", "class", "module"
};

// Weighted directed graph using adjacency list representation
class Graph {
public:
    int add_node(const GraphNode& node);
    void add_edge(int from, int to, EdgeType type, double weight = 1.0);

    const GraphNode& get_node(int id) const;
    const std::vector<Edge>& get_edges(int node_id) const;
    int node_count() const;
    int edge_count() const;

    // Lookup node by name (returns -1 if not found)
    int find_node(const std::string& name) const;

    // Get all node IDs
    std::vector<int> all_node_ids() const;

    // Get incoming edges for a node
    std::vector<Edge> get_incoming_edges(int node_id) const;

    bool has_edge(int from, int to) const;

private:
    std::vector<GraphNode> nodes_;
    std::vector<std::vector<Edge>> adjacency_;
    std::unordered_map<std::string, int> name_index_;
    int total_edges_ = 0;
};

} // namespace probe
