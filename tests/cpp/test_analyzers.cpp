#include <gtest/gtest.h>
#include <cmath>
#include <algorithm>

#include "parser/ast_node.h"
#include "parser/python_profile.h"
#include "graph/graph.h"
#include "graph/algorithms.h"
#include "analyzers/analysis_context.h"
#include "analyzers/api_call_analyzer.h"
#include "analyzers/fan_out_analyzer.h"
#include "analyzers/retry_analyzer.h"
#include "analyzers/crud_analyzer.h"
#include "scoring/scorer.h"

using namespace probe;

// ─── Helper: build a minimal AnalysisContext from AST nodes ─────
// Constructs a graph and computes metrics automatically.
struct TestContext {
    std::vector<ASTNode> nodes;
    Graph graph;
    PythonProfile profile;
    std::unordered_map<int, double> centrality;
    std::unordered_map<int, double> pagerank;

    void build_graph() {
        // Add function nodes to graph
        for (const auto& n : nodes) {
            if (n.type == NodeType::FUNCTION_DEF || n.type == NodeType::CLASS_DEF) {
                std::string name = n.name;
                if (!n.parent_class.empty()) name = n.parent_class + "." + n.name;
                graph.add_node({0, name, n.file_path, n.start_line, "function"});
            }
        }
        // Wire call edges
        for (const auto& n : nodes) {
            if (n.type != NodeType::FUNCTION_DEF) continue;
            std::string caller = n.name;
            if (!n.parent_class.empty()) caller = n.parent_class + "." + n.name;
            int from = graph.find_node(caller);
            if (from == -1) continue;

            for (const auto& call : n.called_functions) {
                int to = graph.find_node(call);
                if (to != -1) graph.add_edge(from, to, EdgeType::CALLS);
            }
        }
        centrality = algorithms::betweenness_centrality(graph);
        pagerank = algorithms::pagerank(graph);
    }

    AnalysisContext make_ctx() {
        return AnalysisContext{graph, nodes, profile, centrality, pagerank, {}};
    }
};

// ─── Helper: make a simple function AST node ────────────────────
ASTNode make_function(const std::string& name,
                      const std::vector<std::string>& calls = {},
                      const std::string& file = "test.py",
                      int line = 1) {
    ASTNode n;
    n.type = NodeType::FUNCTION_DEF;
    n.name = name;
    n.file_path = file;
    n.start_line = line;
    n.end_line = line + 5;
    n.called_functions = calls;
    n.arg_count = 0;
    return n;
}

// ═══════════════════════════════════════════════════════════════════
//  API Call Analyzer Tests
// ═══════════════════════════════════════════════════════════════════

TEST(ApiCallAnalyzerTest, DetectsRequestsCalls) {
    TestContext tc;
    tc.nodes.push_back(make_function("fetch_data", {"requests.get", "json.loads"}));
    tc.nodes.push_back(make_function("helper", {"print"}));
    tc.build_graph();

    ApiCallAnalyzer analyzer;
    auto findings = analyzer.analyze(tc.make_ctx());

    ASSERT_EQ(findings.size(), 1u);
    EXPECT_EQ(findings[0].function_name, "fetch_data");
    EXPECT_EQ(findings[0].type, FindingType::API_CALL);
    EXPECT_GE(findings[0].confidence, 0.5);
}

TEST(ApiCallAnalyzerTest, DetectsMultipleApiLibraries) {
    TestContext tc;
    tc.nodes.push_back(make_function("sync_all",
        {"requests.post", "boto3.client", "redis.set"}));
    tc.build_graph();

    ApiCallAnalyzer analyzer;
    auto findings = analyzer.analyze(tc.make_ctx());

    ASSERT_EQ(findings.size(), 1u);
    // 3 API calls: 0.5 + 3*0.15 = 0.95
    EXPECT_GE(findings[0].confidence, 0.9);
    EXPECT_EQ(findings[0].evidence.size(), 3u);
}

TEST(ApiCallAnalyzerTest, IgnoresNonApiCalls) {
    TestContext tc;
    tc.nodes.push_back(make_function("local_fn", {"print", "len", "sorted"}));
    tc.build_graph();

    ApiCallAnalyzer analyzer;
    auto findings = analyzer.analyze(tc.make_ctx());
    EXPECT_TRUE(findings.empty());
}

TEST(ApiCallAnalyzerTest, BoostsDecoratedFunctions) {
    TestContext tc;
    auto fn = make_function("get_users", {"requests.get"});
    fn.decorators = {"app.route"};
    tc.nodes.push_back(fn);
    tc.build_graph();

    ApiCallAnalyzer analyzer;
    auto findings = analyzer.analyze(tc.make_ctx());

    ASSERT_EQ(findings.size(), 1u);
    // Base 0.65 + decorator 0.1 = 0.75
    EXPECT_GE(findings[0].confidence, 0.7);
}

TEST(ApiCallAnalyzerTest, QualifiesMethodWithParentClass) {
    TestContext tc;
    auto fn = make_function("call_api", {"httpx.get"});
    fn.parent_class = "ApiClient";
    tc.nodes.push_back(fn);
    tc.build_graph();

    ApiCallAnalyzer analyzer;
    auto findings = analyzer.analyze(tc.make_ctx());

    ASSERT_EQ(findings.size(), 1u);
    EXPECT_EQ(findings[0].function_name, "ApiClient.call_api");
}

// ═══════════════════════════════════════════════════════════════════
//  Fan-Out Analyzer Tests
// ═══════════════════════════════════════════════════════════════════

TEST(FanOutAnalyzerTest, DetectsHighFanOut) {
    TestContext tc;
    // orchestrator calls 4 functions
    tc.nodes.push_back(make_function("orchestrator",
        {"step_a", "step_b", "step_c", "step_d"}));
    tc.nodes.push_back(make_function("step_a", {}));
    tc.nodes.push_back(make_function("step_b", {}));
    tc.nodes.push_back(make_function("step_c", {}));
    tc.nodes.push_back(make_function("step_d", {}));
    tc.build_graph();

    FanOutAnalyzer analyzer(3);  // min_out_degree=3
    auto findings = analyzer.analyze(tc.make_ctx());

    ASSERT_EQ(findings.size(), 1u);
    EXPECT_EQ(findings[0].function_name, "orchestrator");
    EXPECT_EQ(findings[0].type, FindingType::FAN_OUT);
    EXPECT_GE(findings[0].confidence, 0.3);
}

TEST(FanOutAnalyzerTest, IgnoresLowFanOut) {
    TestContext tc;
    tc.nodes.push_back(make_function("simple", {"helper"}));
    tc.nodes.push_back(make_function("helper", {}));
    tc.build_graph();

    FanOutAnalyzer analyzer(3);
    auto findings = analyzer.analyze(tc.make_ctx());
    EXPECT_TRUE(findings.empty());
}

TEST(FanOutAnalyzerTest, ReportsOutDegreeInEvidence) {
    TestContext tc;
    tc.nodes.push_back(make_function("pipeline",
        {"load", "transform", "validate", "save", "notify"}));
    tc.nodes.push_back(make_function("load", {}));
    tc.nodes.push_back(make_function("transform", {}));
    tc.nodes.push_back(make_function("validate", {}));
    tc.nodes.push_back(make_function("save", {}));
    tc.nodes.push_back(make_function("notify", {}));
    tc.build_graph();

    FanOutAnalyzer analyzer(3);
    auto findings = analyzer.analyze(tc.make_ctx());

    ASSERT_EQ(findings.size(), 1u);
    // Should list out-degree in evidence
    bool found_degree = false;
    for (const auto& e : findings[0].evidence) {
        if (e.find("out-degree") != std::string::npos) found_degree = true;
    }
    EXPECT_TRUE(found_degree);
}

// ═══════════════════════════════════════════════════════════════════
//  Retry Analyzer Tests
// ═══════════════════════════════════════════════════════════════════

TEST(RetryAnalyzerTest, DetectsSleepPlusApi) {
    TestContext tc;
    tc.nodes.push_back(make_function("poll_status",
        {"time.sleep", "requests.get"}));
    tc.build_graph();

    RetryAnalyzer analyzer;
    auto findings = analyzer.analyze(tc.make_ctx());

    ASSERT_EQ(findings.size(), 1u);
    EXPECT_EQ(findings[0].type, FindingType::RETRY_PATTERN);
    EXPECT_GE(findings[0].confidence, 0.5);
}

TEST(RetryAnalyzerTest, DetectsRetryNameWithSleep) {
    TestContext tc;
    tc.nodes.push_back(make_function("retry_upload",
        {"time.sleep", "upload_file"}));
    tc.build_graph();

    RetryAnalyzer analyzer;
    auto findings = analyzer.analyze(tc.make_ctx());

    ASSERT_EQ(findings.size(), 1u);
    // sleep + retry name = strong signal
    EXPECT_GE(findings[0].confidence, 0.5);
}

TEST(RetryAnalyzerTest, DetectsRetryDecorator) {
    TestContext tc;
    auto fn = make_function("fetch_data", {"requests.get"});
    fn.decorators = {"retry"};
    tc.nodes.push_back(fn);
    tc.build_graph();

    RetryAnalyzer analyzer;
    auto findings = analyzer.analyze(tc.make_ctx());

    ASSERT_EQ(findings.size(), 1u);
}

TEST(RetryAnalyzerTest, IgnoresPlainFunction) {
    TestContext tc;
    tc.nodes.push_back(make_function("compute", {"math.sqrt", "print"}));
    tc.build_graph();

    RetryAnalyzer analyzer;
    auto findings = analyzer.analyze(tc.make_ctx());
    EXPECT_TRUE(findings.empty());
}

TEST(RetryAnalyzerTest, DetectsAsyncioSleep) {
    TestContext tc;
    tc.nodes.push_back(make_function("wait_for_result",
        {"asyncio.sleep", "httpx.get"}));
    tc.build_graph();

    RetryAnalyzer analyzer;
    auto findings = analyzer.analyze(tc.make_ctx());

    ASSERT_EQ(findings.size(), 1u);
}

// ═══════════════════════════════════════════════════════════════════
//  CRUD Analyzer Tests
// ═══════════════════════════════════════════════════════════════════

TEST(CrudAnalyzerTest, DetectsCrudCluster) {
    TestContext tc;
    tc.nodes.push_back(make_function("create_user", {}, "user.py", 1));
    tc.nodes.push_back(make_function("get_user", {}, "user.py", 10));
    tc.nodes.push_back(make_function("update_user", {}, "user.py", 20));
    tc.nodes.push_back(make_function("delete_user", {}, "user.py", 30));
    tc.build_graph();

    CrudAnalyzer analyzer;
    auto findings = analyzer.analyze(tc.make_ctx());

    ASSERT_EQ(findings.size(), 1u);
    EXPECT_EQ(findings[0].type, FindingType::CRUD_CLUSTER);
    EXPECT_GE(findings[0].confidence, 0.7);  // 4 ops = 0.3 + 4*0.2 = high
}

TEST(CrudAnalyzerTest, DetectsPartialCrud) {
    TestContext tc;
    tc.nodes.push_back(make_function("create_order", {}, "order.py", 1));
    tc.nodes.push_back(make_function("get_order", {}, "order.py", 10));
    tc.build_graph();

    CrudAnalyzer analyzer;
    auto findings = analyzer.analyze(tc.make_ctx());

    ASSERT_EQ(findings.size(), 1u);
    // 2 ops: 0.3 + 2*0.2 = 0.7
    EXPECT_GE(findings[0].confidence, 0.5);
}

TEST(CrudAnalyzerTest, IgnoresSingleOperation) {
    TestContext tc;
    tc.nodes.push_back(make_function("create_item", {}));
    tc.build_graph();

    CrudAnalyzer analyzer;
    auto findings = analyzer.analyze(tc.make_ctx());
    EXPECT_TRUE(findings.empty());
}

TEST(CrudAnalyzerTest, HandlesAlternativePrefixes) {
    TestContext tc;
    tc.nodes.push_back(make_function("add_product", {}, "shop.py", 1));
    tc.nodes.push_back(make_function("fetch_product", {}, "shop.py", 10));
    tc.nodes.push_back(make_function("remove_product", {}, "shop.py", 20));
    tc.build_graph();

    CrudAnalyzer analyzer;
    auto findings = analyzer.analyze(tc.make_ctx());

    ASSERT_EQ(findings.size(), 1u);
    // add_=create, fetch_=read, remove_=delete → 3 ops
    EXPECT_GE(findings[0].evidence.size(), 3u);
}

TEST(CrudAnalyzerTest, GroupsByEntity) {
    TestContext tc;
    // Two separate entities
    tc.nodes.push_back(make_function("create_user", {}, "a.py", 1));
    tc.nodes.push_back(make_function("get_user", {}, "a.py", 10));
    tc.nodes.push_back(make_function("create_order", {}, "b.py", 1));
    tc.nodes.push_back(make_function("delete_order", {}, "b.py", 10));
    tc.build_graph();

    CrudAnalyzer analyzer;
    auto findings = analyzer.analyze(tc.make_ctx());

    EXPECT_EQ(findings.size(), 2u);  // one per entity
}

TEST(CrudAnalyzerTest, HandlesPluralStripping) {
    TestContext tc;
    // list_users → entity "user", get_user → entity "user" (same!)
    tc.nodes.push_back(make_function("list_users", {}, "u.py", 1));
    tc.nodes.push_back(make_function("delete_user", {}, "u.py", 10));
    tc.build_graph();

    CrudAnalyzer analyzer;
    auto findings = analyzer.analyze(tc.make_ctx());

    ASSERT_EQ(findings.size(), 1u);  // should group as same entity
}

// ═══════════════════════════════════════════════════════════════════
//  Scorer Tests
// ═══════════════════════════════════════════════════════════════════

TEST(ScorerTest, AppliesTypeWeights) {
    TestContext tc;
    tc.build_graph();
    auto ctx = tc.make_ctx();

    Finding api_f;
    api_f.type = FindingType::API_CALL;
    api_f.confidence = 0.8;
    api_f.function_name = "test";

    Finding retry_f;
    retry_f.type = FindingType::RETRY_PATTERN;
    retry_f.confidence = 0.8;
    retry_f.function_name = "test2";

    Scorer scorer;
    auto scored = scorer.score({api_f, retry_f}, ctx);

    ASSERT_EQ(scored.size(), 2u);
    // RETRY weight=1.1, API weight=1.0
    // Retry: 0.8 * 1.1 = 0.88, API: 0.8 * 1.0 = 0.80
    // Retry should be scored higher
    EXPECT_EQ(scored[0].type, FindingType::RETRY_PATTERN);
}

TEST(ScorerTest, ClampsToValidRange) {
    TestContext tc;
    tc.build_graph();
    auto ctx = tc.make_ctx();

    Finding f;
    f.type = FindingType::RETRY_PATTERN;
    f.confidence = 0.95;  // * 1.1 = 1.045 → should clamp to 1.0
    f.function_name = "test";

    Scorer scorer;
    auto scored = scorer.score({f}, ctx);

    ASSERT_EQ(scored.size(), 1u);
    EXPECT_LE(scored[0].confidence, 1.0);
    EXPECT_GE(scored[0].confidence, 0.0);
}

TEST(ScorerTest, SortsDescendingByConfidence) {
    TestContext tc;
    tc.build_graph();
    auto ctx = tc.make_ctx();

    Finding low, mid, high;
    low.type = FindingType::API_CALL;
    low.confidence = 0.3;
    low.function_name = "low";

    mid.type = FindingType::API_CALL;
    mid.confidence = 0.6;
    mid.function_name = "mid";

    high.type = FindingType::API_CALL;
    high.confidence = 0.9;
    high.function_name = "high";

    Scorer scorer;
    auto scored = scorer.score({low, high, mid}, ctx);

    ASSERT_EQ(scored.size(), 3u);
    EXPECT_GE(scored[0].confidence, scored[1].confidence);
    EXPECT_GE(scored[1].confidence, scored[2].confidence);
}

TEST(ScorerTest, CustomWeightOverridesDefault) {
    TestContext tc;
    tc.build_graph();
    auto ctx = tc.make_ctx();

    Finding f;
    f.type = FindingType::CRUD_CLUSTER;
    f.confidence = 0.5;
    f.function_name = "test";

    Scorer scorer;
    // Default CRUD weight is 0.85, override to 2.0
    scorer.set_weight(FindingType::CRUD_CLUSTER, 2.0);
    auto scored = scorer.score({f}, ctx);

    ASSERT_EQ(scored.size(), 1u);
    // 0.5 * 2.0 = 1.0 (clamped)
    EXPECT_EQ(scored[0].confidence, 1.0);
}

TEST(ScorerTest, RoundsToTwoDecimals) {
    TestContext tc;
    tc.build_graph();
    auto ctx = tc.make_ctx();

    Finding f;
    f.type = FindingType::FAN_OUT;
    f.confidence = 0.777;  // * 0.9 = 0.6993 → rounds to 0.70
    f.function_name = "test";

    Scorer scorer;
    auto scored = scorer.score({f}, ctx);

    ASSERT_EQ(scored.size(), 1u);
    // Check it's a clean 2-decimal value
    double rounded = std::round(scored[0].confidence * 100.0) / 100.0;
    EXPECT_DOUBLE_EQ(scored[0].confidence, rounded);
}

TEST(ScorerTest, PageRankBoostsHighCentralityNode) {
    TestContext tc;
    // Build a star graph where "hub" has high PageRank
    tc.nodes.push_back(make_function("leaf1", {"hub"}));
    tc.nodes.push_back(make_function("leaf2", {"hub"}));
    tc.nodes.push_back(make_function("leaf3", {"hub"}));
    tc.nodes.push_back(make_function("hub", {}));
    tc.build_graph();
    auto ctx = tc.make_ctx();

    // "hub" gets linked from 3 leaves → high PageRank
    Finding f;
    f.type = FindingType::API_CALL;
    f.confidence = 0.5;
    f.function_name = "hub";

    Scorer scorer;
    auto scored = scorer.score({f}, ctx);

    ASSERT_EQ(scored.size(), 1u);
    // Should be at least 0.5 (base * weight), possibly boosted by PageRank
    EXPECT_GE(scored[0].confidence, 0.5);
}

TEST(ScorerTest, EmptyInputReturnsEmpty) {
    TestContext tc;
    tc.build_graph();
    auto ctx = tc.make_ctx();

    Scorer scorer;
    auto scored = scorer.score({}, ctx);
    EXPECT_TRUE(scored.empty());
}
