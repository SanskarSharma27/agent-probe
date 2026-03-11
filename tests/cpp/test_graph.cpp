#include <gtest/gtest.h>
#include <cmath>
#include <algorithm>
#include "graph/graph.h"
#include "graph/builder.h"
#include "graph/algorithms.h"
#include "parser/ts_parser.h"
#include "parser/python_profile.h"

extern "C" const TSLanguage *tree_sitter_python();

using namespace probe;

// ─── Graph data structure tests ────────────────────────────────

TEST(GraphTest, AddNodesAndEdges) {
    Graph g;
    int a = g.add_node({0, "A", "f.py", 1, "function"});
    int b = g.add_node({0, "B", "f.py", 2, "function"});
    int c = g.add_node({0, "C", "f.py", 3, "function"});

    g.add_edge(a, b, EdgeType::CALLS);
    g.add_edge(a, c, EdgeType::CALLS);

    EXPECT_EQ(g.node_count(), 3);
    EXPECT_EQ(g.edge_count(), 2);
    EXPECT_EQ(g.get_edges(a).size(), 2u);
    EXPECT_EQ(g.get_edges(b).size(), 0u);
}

TEST(GraphTest, FindNodeByName) {
    Graph g;
    g.add_node({0, "alpha", "f.py", 1, "function"});
    g.add_node({0, "beta", "f.py", 2, "function"});

    EXPECT_EQ(g.find_node("alpha"), 0);
    EXPECT_EQ(g.find_node("beta"), 1);
    EXPECT_EQ(g.find_node("gamma"), -1);
}

TEST(GraphTest, HasEdge) {
    Graph g;
    int a = g.add_node({0, "A", "", 0, ""});
    int b = g.add_node({0, "B", "", 0, ""});

    EXPECT_FALSE(g.has_edge(a, b));
    g.add_edge(a, b, EdgeType::CALLS);
    EXPECT_TRUE(g.has_edge(a, b));
    EXPECT_FALSE(g.has_edge(b, a));  // directed
}

TEST(GraphTest, IncomingEdges) {
    Graph g;
    int a = g.add_node({0, "A", "", 0, ""});
    int b = g.add_node({0, "B", "", 0, ""});
    int c = g.add_node({0, "C", "", 0, ""});

    g.add_edge(a, c, EdgeType::CALLS);
    g.add_edge(b, c, EdgeType::CALLS);

    auto incoming = g.get_incoming_edges(c);
    EXPECT_EQ(incoming.size(), 2u);
}

// ─── Builder tests ─────────────────────────────────────────────

TEST(BuilderTest, BuildsFromSimpleCode) {
    TSParserWrapper parser;
    parser.set_language(tree_sitter_python());
    PythonProfile profile;

    std::string code =
        "def foo():\n"
        "    bar()\n\n"
        "def bar():\n"
        "    baz()\n\n"
        "def baz():\n"
        "    pass\n";

    auto nodes = parser.parse_string(code, "test.py", profile);
    GraphBuilder builder;
    Graph g = builder.build(nodes);

    EXPECT_EQ(g.node_count(), 3);
    EXPECT_TRUE(g.has_edge(g.find_node("foo"), g.find_node("bar")));
    EXPECT_TRUE(g.has_edge(g.find_node("bar"), g.find_node("baz")));
    EXPECT_FALSE(g.has_edge(g.find_node("baz"), g.find_node("foo")));
}

TEST(BuilderTest, BuildsFromFlaskFixture) {
    TSParserWrapper parser;
    parser.set_language(tree_sitter_python());
    PythonProfile profile;

    auto nodes = parser.parse_file("fixtures/python/flask_app.py", profile);
    GraphBuilder builder;
    Graph g = builder.build(nodes);

    // Should have nodes for classes and functions
    EXPECT_GT(g.node_count(), 5);
    EXPECT_GT(g.edge_count(), 0);

    // list_users should call user_service.get_user (resolved to UserService.get_user)
    int list_fn = g.find_node("list_users");
    EXPECT_NE(list_fn, -1);

    // DatabaseClient and UserService should exist as class nodes
    EXPECT_NE(g.find_node("DatabaseClient"), -1);
    EXPECT_NE(g.find_node("UserService"), -1);
}

TEST(BuilderTest, BuildsFromPipelineFixture) {
    TSParserWrapper parser;
    parser.set_language(tree_sitter_python());
    PythonProfile profile;

    auto nodes = parser.parse_file("fixtures/python/data_pipeline.py", profile);
    GraphBuilder builder;
    Graph g = builder.build(nodes);

    // run_pipeline → load_data, transform, save_results
    int pipeline = g.find_node("run_pipeline");
    int load = g.find_node("load_data");
    int transform = g.find_node("transform");
    int save = g.find_node("save_results");

    ASSERT_NE(pipeline, -1);
    ASSERT_NE(load, -1);
    ASSERT_NE(transform, -1);
    ASSERT_NE(save, -1);

    EXPECT_TRUE(g.has_edge(pipeline, load));
    EXPECT_TRUE(g.has_edge(pipeline, transform));
    EXPECT_TRUE(g.has_edge(pipeline, save));

    // transform → clean_text, validate_record
    int clean = g.find_node("clean_text");
    int validate = g.find_node("validate_record");
    ASSERT_NE(clean, -1);
    ASSERT_NE(validate, -1);
    EXPECT_TRUE(g.has_edge(transform, clean));
    EXPECT_TRUE(g.has_edge(transform, validate));
}

// ─── Algorithm tests (hand-built graphs) ───────────────────────
//
// Test graph (diamond):
//       0
//      / \
//     1   2
//      \ /
//       3
//
Graph make_diamond() {
    Graph g;
    g.add_node({0, "A", "", 0, ""});
    g.add_node({0, "B", "", 0, ""});
    g.add_node({0, "C", "", 0, ""});
    g.add_node({0, "D", "", 0, ""});
    g.add_edge(0, 1, EdgeType::CALLS);
    g.add_edge(0, 2, EdgeType::CALLS);
    g.add_edge(1, 3, EdgeType::CALLS);
    g.add_edge(2, 3, EdgeType::CALLS);
    return g;
}

// Linear chain: 0 → 1 → 2 → 3 → 4
Graph make_chain() {
    Graph g;
    for (int i = 0; i < 5; i++) {
        g.add_node({0, "N" + std::to_string(i), "", 0, ""});
    }
    for (int i = 0; i < 4; i++) {
        g.add_edge(i, i + 1, EdgeType::CALLS);
    }
    return g;
}

// Star: 0 → {1, 2, 3, 4}
Graph make_star() {
    Graph g;
    for (int i = 0; i < 5; i++) {
        g.add_node({0, "N" + std::to_string(i), "", 0, ""});
    }
    for (int i = 1; i < 5; i++) {
        g.add_edge(0, i, EdgeType::CALLS);
    }
    return g;
}

TEST(AlgorithmTest, BFS_Diamond) {
    Graph g = make_diamond();
    auto order = algorithms::bfs(g, 0);

    ASSERT_EQ(order.size(), 4u);
    EXPECT_EQ(order[0], 0);           // start
    // 1 and 2 should come before 3 (they're at depth 1)
    EXPECT_TRUE(order[1] == 1 || order[1] == 2);
    EXPECT_TRUE(order[2] == 1 || order[2] == 2);
    EXPECT_EQ(order[3], 3);           // depth 2
}

TEST(AlgorithmTest, DFS_Diamond) {
    Graph g = make_diamond();
    auto order = algorithms::dfs(g, 0);

    ASSERT_EQ(order.size(), 4u);
    EXPECT_EQ(order[0], 0);  // always starts at source
}

TEST(AlgorithmTest, BFS_Chain) {
    Graph g = make_chain();
    auto order = algorithms::bfs(g, 0);

    // Should visit in order 0, 1, 2, 3, 4
    ASSERT_EQ(order.size(), 5u);
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(order[i], i);
    }
}

TEST(AlgorithmTest, BFS_UnreachableNodes) {
    Graph g;
    g.add_node({0, "A", "", 0, ""});
    g.add_node({0, "B", "", 0, ""});
    // No edges — B unreachable from A
    auto order = algorithms::bfs(g, 0);
    EXPECT_EQ(order.size(), 1u);
}

TEST(AlgorithmTest, BetweennessCentrality_Chain) {
    Graph g = make_chain();  // 0 → 1 → 2 → 3 → 4
    auto bc = algorithms::betweenness_centrality(g);

    // In a directed chain, node 1 is on the path 0→2, 0→3, 0→4 (3 paths)
    // Node 2 is on paths 0→3, 0→4, 1→3, 1→4 (4 paths) — but only shortest
    // Node 2 should have highest centrality (middle of chain)
    // Endpoints should have 0 centrality
    EXPECT_DOUBLE_EQ(bc[0], 0.0);
    EXPECT_DOUBLE_EQ(bc[4], 0.0);
    EXPECT_GT(bc[1], 0.0);
    EXPECT_GT(bc[2], 0.0);
    EXPECT_GT(bc[3], 0.0);

    // Middle nodes should have higher centrality than near-endpoints
    EXPECT_GE(bc[2], bc[1]);
    EXPECT_GE(bc[2], bc[3]);
}

TEST(AlgorithmTest, BetweennessCentrality_Diamond) {
    Graph g = make_diamond();
    auto bc = algorithms::betweenness_centrality(g);

    // Node 0 (source) and 3 (sink) should have 0 betweenness
    // Nodes 1 and 2 are on alternative paths, so their centrality
    // reflects they each carry half the traffic
    EXPECT_DOUBLE_EQ(bc[0], 0.0);
    EXPECT_DOUBLE_EQ(bc[3], 0.0);
    EXPECT_DOUBLE_EQ(bc[1], bc[2]);  // symmetric
}

TEST(AlgorithmTest, PageRank_Star) {
    Graph g = make_star();  // 0 → {1,2,3,4}
    auto pr = algorithms::pagerank(g, 0.85, 100);

    // All leaf nodes should have equal rank
    EXPECT_NEAR(pr[1], pr[2], 1e-6);
    EXPECT_NEAR(pr[2], pr[3], 1e-6);
    EXPECT_NEAR(pr[3], pr[4], 1e-6);

    // Leaf nodes receive rank from hub, so they should have more
    // than the hub (which only gets the random teleport share)
    EXPECT_GT(pr[1], pr[0]);
}

TEST(AlgorithmTest, PageRank_SumsToOne) {
    Graph g = make_diamond();
    auto pr = algorithms::pagerank(g, 0.85, 100);

    double sum = 0.0;
    for (auto& [id, val] : pr) sum += val;
    EXPECT_NEAR(sum, 1.0, 1e-6);
}

TEST(AlgorithmTest, Degree_Star) {
    Graph g = make_star();

    EXPECT_EQ(algorithms::out_degree(g, 0), 4);
    EXPECT_EQ(algorithms::in_degree(g, 0), 0);
    EXPECT_EQ(algorithms::out_degree(g, 1), 0);
    EXPECT_EQ(algorithms::in_degree(g, 1), 1);
}

TEST(AlgorithmTest, Reachability) {
    Graph g = make_chain();
    auto reachable = algorithms::reachable_from(g, 0);
    EXPECT_EQ(reachable.size(), 4u);  // 1,2,3,4

    auto from_last = algorithms::reachable_from(g, 4);
    EXPECT_EQ(from_last.size(), 0u);  // nothing reachable from end
}
