#include "cli/formatter.h"

#include <sstream>
#include <iomanip>
#include <cstdio>
#include <iostream>
#include <nlohmann/json.hpp>

namespace probe {

// ANSI color codes
static const char* RESET  = "\033[0m";
static const char* BOLD   = "\033[1m";
static const char* DIM    = "\033[2m";
static const char* RED    = "\033[31m";
static const char* GREEN  = "\033[32m";
static const char* YELLOW = "\033[33m";
static const char* CYAN   = "\033[36m";
static const char* WHITE  = "\033[37m";

// Color confidence: green >= 0.7, yellow >= 0.4, red < 0.4
static const char* confidence_color(double c) {
    if (c >= 0.7) return RED;      // high confidence = high priority (red = attention)
    if (c >= 0.4) return YELLOW;
    return GREEN;                   // low confidence = low concern
}

// Color by finding type
static const char* type_color(FindingType t) {
    switch (t) {
        case FindingType::API_CALL:      return CYAN;
        case FindingType::FAN_OUT:       return YELLOW;
        case FindingType::RETRY_PATTERN: return RED;
        case FindingType::CRUD_CLUSTER:  return GREEN;
    }
    return WHITE;
}

static std::string edge_type_str(EdgeType t) {
    switch (t) {
        case EdgeType::CALLS:    return "calls";
        case EdgeType::IMPORTS:  return "imports";
        case EdgeType::INHERITS: return "inherits";
        case EdgeType::CONTAINS: return "contains";
    }
    return "unknown";
}

std::string format_graph_json(const std::vector<Finding>& findings, const ScanStats& stats,
                               const Graph& graph,
                               const std::unordered_map<int, double>& pagerank) {
    nlohmann::json output;
    output["version"] = stats.version;
    output["path"] = stats.path;
    output["files_scanned"] = stats.files_scanned;

    // Graph nodes
    nlohmann::json nodes_json = nlohmann::json::array();
    for (int id : graph.all_node_ids()) {
        const auto& node = graph.get_node(id);
        nlohmann::json nj;
        nj["id"] = id;
        nj["name"] = node.name;
        nj["file"] = node.file_path;
        nj["line"] = node.line_number;
        nj["type"] = node.node_type;

        auto pr_it = pagerank.find(id);
        nj["pagerank"] = (pr_it != pagerank.end()) ? pr_it->second : 0.0;

        // Check if this node has any findings
        nj["finding_types"] = nlohmann::json::array();
        for (const auto& f : findings) {
            if (f.function_name == node.name ||
                f.function_name.find(node.name) != std::string::npos) {
                nj["finding_types"].push_back(finding_type_str(f.type));
            }
        }

        nodes_json.push_back(nj);
    }
    output["nodes"] = nodes_json;

    // Graph edges
    nlohmann::json edges_json = nlohmann::json::array();
    for (int id : graph.all_node_ids()) {
        for (const auto& edge : graph.get_edges(id)) {
            nlohmann::json ej;
            ej["source"] = id;
            ej["target"] = edge.target;
            ej["type"] = edge_type_str(edge.type);
            ej["weight"] = edge.weight;
            edges_json.push_back(ej);
        }
    }
    output["edges"] = edges_json;

    // Findings
    nlohmann::json findings_json = nlohmann::json::array();
    for (const auto& f : findings) {
        nlohmann::json fj;
        fj["file"] = f.file_path;
        fj["line"] = f.line_number;
        fj["function"] = f.function_name;
        fj["type"] = finding_type_str(f.type);
        fj["confidence"] = f.confidence;
        fj["reason"] = f.reason;
        fj["evidence"] = f.evidence;
        findings_json.push_back(fj);
    }
    output["findings"] = findings_json;

    return output.dump(2);
}

// ─── Per-type agent integration metadata ─────────────────────────

struct AgentPattern {
    const char* suggested_role;
    const char* rationale;
    const char* langchain_pattern;
};

static AgentPattern agent_pattern_for(FindingType type) {
    switch (type) {
        case FindingType::RETRY_PATTERN:
            return {
                "Autonomous retry agent with configurable backoff and dead-letter queue",
                "Function already handles retry logic manually. An agent replaces brittle sleep/loop with observable, configurable backoff and failure routing.",
                "from langchain.tools import tool\nfrom langchain.agents import AgentExecutor"
            };
        case FindingType::API_CALL:
            return {
                "Boundary agent with caching, rate limiting, and fallback logic",
                "Function crosses a service boundary via external API call. An agent at this boundary can add intelligent caching, automatic rate limit handling, and graceful fallback to stale data.",
                "from langchain.tools import tool\nfrom langchain.cache import InMemoryCache"
            };
        case FindingType::FAN_OUT:
            return {
                "Orchestration agent that parallelizes calls and handles partial failures",
                "Function coordinates multiple downstream calls. An agent can parallelize independent calls, handle partial failures gracefully, and add distributed tracing.",
                "from langchain.agents import AgentExecutor\nfrom langchain.tools import StructuredTool"
            };
        case FindingType::CRUD_CLUSTER:
            return {
                "Data lifecycle agent with event emission and audit logging",
                "Functions manage a data entity lifecycle (create/read/update/delete). An agent can add event-driven workflows, audit trails, and mutation validation.",
                "from langchain.tools import tool\nfrom langchain.callbacks import StdOutCallbackHandler"
            };
    }
    return {"Unknown", "", ""};
}

std::string format_json(const std::vector<Finding>& findings, const ScanStats& stats) {
    nlohmann::json output;
    output["version"] = stats.version;
    output["path"] = stats.path;
    output["files_scanned"] = stats.files_scanned;
    output["nodes_parsed"] = stats.nodes_parsed;
    output["graph_nodes"] = stats.graph_nodes;
    output["graph_edges"] = stats.graph_edges;
    output["total_findings"] = stats.total_findings;
    output["shown_findings"] = static_cast<int>(findings.size());

    nlohmann::json findings_json = nlohmann::json::array();
    for (const auto& f : findings) {
        nlohmann::json fj;
        fj["file"] = f.file_path;
        fj["line"] = f.line_number;
        fj["function"] = f.function_name;
        fj["type"] = finding_type_str(f.type);
        fj["confidence"] = f.confidence;
        fj["reason"] = f.reason;
        fj["evidence"] = f.evidence;

        auto pattern = agent_pattern_for(f.type);
        fj["suggested_role"] = pattern.suggested_role;
        fj["rationale"] = pattern.rationale;
        fj["framework"] = "langchain";

        findings_json.push_back(fj);
    }
    output["findings"] = findings_json;

    return output.dump(2);
}

std::string format_table(const std::vector<Finding>& findings, const ScanStats& stats) {
    std::ostringstream out;

    out << "agent-probe v" << stats.version << " — "
        << stats.files_scanned << " files, "
        << stats.graph_nodes << " graph nodes\n\n";

    if (findings.empty()) {
        out << "No findings.\n";
        return out.str();
    }

    // Header
    char header[256], separator[256];
    std::snprintf(header, sizeof(header), "%-6s  %-12s  %-8s  %-30s  %s",
                  "CONF", "TYPE", "LINE", "FUNCTION", "FILE");
    std::snprintf(separator, sizeof(separator), "%-6s  %-12s  %-8s  %-30s  %s",
                  "------", "------------", "--------",
                  "------------------------------",
                  "--------------------");
    out << header << "\n" << separator << "\n";

    for (const auto& f : findings) {
        char line[512];
        std::snprintf(line, sizeof(line), "%-6.2f  %-12s  %-8d  %-30s  %s",
                      f.confidence,
                      finding_type_str(f.type).c_str(),
                      f.line_number,
                      f.function_name.c_str(),
                      f.file_path.c_str());
        out << line << "\n";
    }

    out << "\n" << findings.size() << " finding(s)\n";
    return out.str();
}

std::string format_summary(const std::vector<Finding>& findings, const ScanStats& stats) {
    std::ostringstream out;

    out << "agent-probe v" << stats.version << "\n";
    out << "Scanned " << stats.files_scanned << " files, "
        << stats.nodes_parsed << " AST nodes\n";
    out << "Graph: " << stats.graph_nodes << " nodes, "
        << stats.graph_edges << " edges\n\n";

    out << "Findings: " << findings.size() << "\n";

    // Count by type
    int api = 0, fan = 0, retry = 0, crud = 0;
    for (const auto& f : findings) {
        switch (f.type) {
            case FindingType::API_CALL: api++; break;
            case FindingType::FAN_OUT: fan++; break;
            case FindingType::RETRY_PATTERN: retry++; break;
            case FindingType::CRUD_CLUSTER: crud++; break;
        }
    }
    if (api)   out << "  API calls:      " << api << "\n";
    if (fan)   out << "  Fan-out:        " << fan << "\n";
    if (retry) out << "  Retry patterns: " << retry << "\n";
    if (crud)  out << "  CRUD clusters:  " << crud << "\n";

    return out.str();
}

std::string format_table_color(const std::vector<Finding>& findings, const ScanStats& stats) {
    std::ostringstream out;

    out << BOLD << "agent-probe" << RESET << " v" << stats.version
        << DIM << " — " << stats.files_scanned << " files, "
        << stats.graph_nodes << " graph nodes" << RESET << "\n\n";

    if (findings.empty()) {
        out << GREEN << "No findings." << RESET << "\n";
        return out.str();
    }

    // Header
    out << BOLD;
    char header[256];
    std::snprintf(header, sizeof(header), "%-6s  %-12s  %-8s  %-30s  %s",
                  "CONF", "TYPE", "LINE", "FUNCTION", "FILE");
    out << header << RESET << "\n";
    out << DIM;
    char sep[256];
    std::snprintf(sep, sizeof(sep), "%-6s  %-12s  %-8s  %-30s  %s",
                  "------", "------------", "--------",
                  "------------------------------",
                  "--------------------");
    out << sep << RESET << "\n";

    for (const auto& f : findings) {
        // Confidence with color
        char conf_buf[16];
        std::snprintf(conf_buf, sizeof(conf_buf), "%-6.2f", f.confidence);
        out << confidence_color(f.confidence) << BOLD << conf_buf << RESET;

        // Type with color
        char type_buf[16];
        std::snprintf(type_buf, sizeof(type_buf), "  %-12s", finding_type_str(f.type).c_str());
        out << type_color(f.type) << type_buf << RESET;

        // Line, function, file
        char rest[512];
        std::snprintf(rest, sizeof(rest), "  %-8d  %-30s  %s",
                      f.line_number,
                      f.function_name.c_str(),
                      f.file_path.c_str());
        out << rest << "\n";
    }

    out << "\n" << BOLD << findings.size() << " finding(s)" << RESET << "\n";
    return out.str();
}

std::string format_plan(const std::vector<Finding>& findings, const ScanStats& stats) {
    std::ostringstream out;

    out << "# Agent Integration Plan\n\n";
    out << "**Source**: `" << stats.path << "`\n";
    out << "**Scanned**: " << stats.files_scanned << " files, "
        << stats.graph_nodes << " graph nodes, "
        << stats.graph_edges << " edges\n";
    out << "**Findings**: " << findings.size() << "\n\n";
    out << "---\n\n";

    if (findings.empty()) {
        out << "No agent integration points detected.\n";
        return out.str();
    }

    // Type summary table
    int api = 0, fan = 0, retry = 0, crud = 0;
    for (const auto& f : findings) {
        switch (f.type) {
            case FindingType::API_CALL: api++; break;
            case FindingType::FAN_OUT: fan++; break;
            case FindingType::RETRY_PATTERN: retry++; break;
            case FindingType::CRUD_CLUSTER: crud++; break;
        }
    }

    out << "## Summary\n\n";
    out << "| Type | Count | Agent Pattern |\n";
    out << "|------|-------|---------------|\n";
    if (retry) out << "| Retry/Polling | " << retry << " | Autonomous retry agent with configurable backoff |\n";
    if (api) out << "| API Boundary | " << api << " | Boundary agent with caching and fallback |\n";
    if (fan) out << "| Fan-Out Hub | " << fan << " | Orchestration agent with parallel execution |\n";
    if (crud) out << "| CRUD Cluster | " << crud << " | Data lifecycle agent with event emission |\n";
    out << "\n---\n\n";

    // Individual findings
    out << "## Integration Points\n\n";

    int idx = 1;
    for (const auto& f : findings) {
        auto pattern = agent_pattern_for(f.type);

        out << "### " << idx << ". " << f.function_name
            << " — `" << f.file_path << ":" << f.line_number << "`\n\n";

        out << "**Type**: " << finding_type_str(f.type)
            << " | **Confidence**: " << std::fixed << std::setprecision(2) << f.confidence << "\n\n";

        out << "**Why**: " << pattern.rationale << "\n\n";

        out << "**Suggested agent role**: " << pattern.suggested_role << "\n\n";

        // Evidence
        out << "**Evidence**:\n";
        for (const auto& e : f.evidence) {
            out << "- " << e << "\n";
        }
        out << "\n";

        // LangChain stub — sanitize function name for valid Python identifier
        std::string safe_name;
        for (char c : f.function_name) {
            if (std::isalnum(c) || c == '_') safe_name += c;
            else if (c == '.' || c == ' ') safe_name += '_';
        }

        out << "**LangChain scaffold**:\n";
        out << "```python\n";
        out << pattern.langchain_pattern << "\n\n";
        out << "@tool\n";
        out << "def " << safe_name << "_agent(input: dict) -> dict:\n";
        out << "    \"\"\"\n";
        out << "    Agent wrapper for " << f.function_name << ".\n";
        out << "    " << pattern.suggested_role << "\n";
        out << "    \"\"\"\n";
        out << "    # TODO: implement agent logic\n";
        out << "    pass\n";
        out << "```\n\n";

        out << "---\n\n";
        idx++;
    }

    out << "*Generated by agent-probe v" << stats.version << "*\n";

    return out.str();
}

void print_progress(const std::string& message, int current, int total) {
    if (total > 0) {
        std::cerr << "\r\033[K" << DIM << "[" << current << "/" << total << "] "
                  << message << "..." << RESET << std::flush;
    } else {
        std::cerr << "\r\033[K" << DIM << message << "..." << RESET << std::flush;
    }
}

void clear_progress() {
    std::cerr << "\r\033[K" << std::flush;
}

} // namespace probe
