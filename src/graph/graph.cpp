#include "graph/graph.h"
#include <stdexcept>

namespace probe {

int Graph::add_node(const GraphNode& node) {
    int id = static_cast<int>(nodes_.size());
    GraphNode n = node;
    n.id = id;
    nodes_.push_back(std::move(n));
    adjacency_.emplace_back();
    name_index_[nodes_[id].name] = id;
    return id;
}

void Graph::add_edge(int from, int to, EdgeType type, double weight) {
    if (from < 0 || from >= static_cast<int>(nodes_.size()) ||
        to < 0 || to >= static_cast<int>(nodes_.size())) {
        return;
    }
    adjacency_[from].push_back({to, type, weight});
    total_edges_++;
}

const GraphNode& Graph::get_node(int id) const {
    return nodes_.at(id);
}

const std::vector<Edge>& Graph::get_edges(int node_id) const {
    return adjacency_.at(node_id);
}

int Graph::node_count() const {
    return static_cast<int>(nodes_.size());
}

int Graph::edge_count() const {
    return total_edges_;
}

int Graph::find_node(const std::string& name) const {
    auto it = name_index_.find(name);
    if (it != name_index_.end()) return it->second;
    return -1;
}

std::vector<int> Graph::all_node_ids() const {
    std::vector<int> ids;
    ids.reserve(nodes_.size());
    for (int i = 0; i < static_cast<int>(nodes_.size()); i++) {
        ids.push_back(i);
    }
    return ids;
}

std::vector<Edge> Graph::get_incoming_edges(int node_id) const {
    std::vector<Edge> incoming;
    for (int i = 0; i < static_cast<int>(adjacency_.size()); i++) {
        for (const auto& edge : adjacency_[i]) {
            if (edge.target == node_id) {
                incoming.push_back({i, edge.type, edge.weight});
            }
        }
    }
    return incoming;
}

bool Graph::has_edge(int from, int to) const {
    if (from < 0 || from >= static_cast<int>(adjacency_.size())) return false;
    for (const auto& edge : adjacency_[from]) {
        if (edge.target == to) return true;
    }
    return false;
}

} // namespace probe
