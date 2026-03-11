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
