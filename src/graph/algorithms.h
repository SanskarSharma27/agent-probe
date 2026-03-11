#pragma once

#include <vector>
#include <unordered_map>

namespace probe {

class Graph;

struct AlgorithmResult {
    std::unordered_map<int, double> centrality_scores;
    std::vector<int> traversal_order;
};

// Graph algorithms for analyzing dependency structure
namespace algorithms {

    // Traversal
    std::vector<int> bfs(const Graph& g, int start);
    std::vector<int> dfs(const Graph& g, int start);

    // Centrality measures
    std::unordered_map<int, double> betweenness_centrality(const Graph& g);
    std::unordered_map<int, double> pagerank(const Graph& g, double damping = 0.85, int iterations = 100);

    // Connectivity
    std::vector<int> reachable_from(const Graph& g, int start);
    int in_degree(const Graph& g, int node);
    int out_degree(const Graph& g, int node);

} // namespace algorithms
} // namespace probe
