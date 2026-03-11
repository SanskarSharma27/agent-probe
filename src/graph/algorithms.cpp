#include "graph/algorithms.h"
#include "graph/graph.h"

#include <queue>
#include <stack>
#include <unordered_set>
#include <algorithm>

namespace probe {
namespace algorithms {

// ─── BFS ───────────────────────────────────────────────────────
std::vector<int> bfs(const Graph& g, int start) {
    std::vector<int> order;
    if (start < 0 || start >= g.node_count()) return order;

    std::vector<bool> visited(g.node_count(), false);
    std::queue<int> frontier;

    visited[start] = true;
    frontier.push(start);

    while (!frontier.empty()) {
        int current = frontier.front();
        frontier.pop();
        order.push_back(current);

        for (const auto& edge : g.get_edges(current)) {
            if (!visited[edge.target]) {
                visited[edge.target] = true;
                frontier.push(edge.target);
            }
        }
    }
    return order;
}

// ─── DFS ───────────────────────────────────────────────────────
std::vector<int> dfs(const Graph& g, int start) {
    std::vector<int> order;
    if (start < 0 || start >= g.node_count()) return order;

    std::vector<bool> visited(g.node_count(), false);
    std::stack<int> frontier;

    frontier.push(start);

    while (!frontier.empty()) {
        int current = frontier.top();
        frontier.pop();

        if (visited[current]) continue;
        visited[current] = true;
        order.push_back(current);

        // Push neighbors in reverse order so leftmost is visited first
        const auto& edges = g.get_edges(current);
        for (int i = static_cast<int>(edges.size()) - 1; i >= 0; i--) {
            if (!visited[edges[i].target]) {
                frontier.push(edges[i].target);
            }
        }
    }
    return order;
}

// ─── Betweenness Centrality (Brandes' algorithm) ──────────────
// For each node s, run BFS to find shortest paths, then backtrack
// to accumulate dependency scores. O(V * E) for unweighted graphs.
std::unordered_map<int, double> betweenness_centrality(const Graph& g) {
    int n = g.node_count();
    std::unordered_map<int, double> cb;

    for (int i = 0; i < n; i++) cb[i] = 0.0;
    if (n <= 1) return cb;

    for (int s = 0; s < n; s++) {
        // BFS from s
        std::stack<int> S;
        std::vector<std::vector<int>> pred(n);
        std::vector<int> sigma(n, 0);     // number of shortest paths
        std::vector<int> dist(n, -1);     // distance from s

        sigma[s] = 1;
        dist[s] = 0;

        std::queue<int> Q;
        Q.push(s);

        while (!Q.empty()) {
            int v = Q.front();
            Q.pop();
            S.push(v);

            for (const auto& edge : g.get_edges(v)) {
                int w = edge.target;
                // First visit?
                if (dist[w] < 0) {
                    dist[w] = dist[v] + 1;
                    Q.push(w);
                }
                // Shortest path via v?
                if (dist[w] == dist[v] + 1) {
                    sigma[w] += sigma[v];
                    pred[w].push_back(v);
                }
            }
        }

        // Back-propagation of dependencies
        std::vector<double> delta(n, 0.0);
        while (!S.empty()) {
            int w = S.top();
            S.pop();
            for (int v : pred[w]) {
                double contrib = (static_cast<double>(sigma[v]) / sigma[w]) * (1.0 + delta[w]);
                delta[v] += contrib;
            }
            if (w != s) {
                cb[w] += delta[w];
            }
        }
    }

    // Normalize for directed graph: no division by 2
    return cb;
}

// ─── PageRank ──────────────────────────────────────────────────
// Iterative PageRank with configurable damping factor.
// Nodes with no outgoing edges distribute rank evenly (dangling nodes).
std::unordered_map<int, double> pagerank(const Graph& g, double damping, int iterations) {
    int n = g.node_count();
    std::unordered_map<int, double> rank;
    if (n == 0) return rank;

    double init_rank = 1.0 / n;
    for (int i = 0; i < n; i++) rank[i] = init_rank;

    for (int iter = 0; iter < iterations; iter++) {
        std::unordered_map<int, double> new_rank;
        double dangling_sum = 0.0;

        // Collect dangling node rank
        for (int i = 0; i < n; i++) {
            if (g.get_edges(i).empty()) {
                dangling_sum += rank[i];
            }
            new_rank[i] = 0.0;
        }

        // Distribute rank from incoming edges
        for (int i = 0; i < n; i++) {
            const auto& edges = g.get_edges(i);
            if (edges.empty()) continue;

            double share = rank[i] / edges.size();
            for (const auto& edge : edges) {
                new_rank[edge.target] += share;
            }
        }

        // Apply damping + distribute dangling rank
        for (int i = 0; i < n; i++) {
            new_rank[i] = (1.0 - damping) / n
                        + damping * (new_rank[i] + dangling_sum / n);
        }

        rank = std::move(new_rank);
    }

    return rank;
}

// ─── Reachability ──────────────────────────────────────────────
std::vector<int> reachable_from(const Graph& g, int start) {
    // BFS but return all reachable nodes (excluding start)
    auto order = bfs(g, start);
    if (!order.empty() && order[0] == start) {
        order.erase(order.begin());
    }
    return order;
}

// ─── Degree ────────────────────────────────────────────────────
int in_degree(const Graph& g, int node) {
    return static_cast<int>(g.get_incoming_edges(node).size());
}

int out_degree(const Graph& g, int node) {
    if (node < 0 || node >= g.node_count()) return 0;
    return static_cast<int>(g.get_edges(node).size());
}

} // namespace algorithms
} // namespace probe
