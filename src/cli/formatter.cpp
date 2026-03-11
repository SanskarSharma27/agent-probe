#include "cli/formatter.h"

#include <sstream>
#include <iomanip>
#include <cstdio>
#include <nlohmann/json.hpp>

namespace probe {

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

} // namespace probe
