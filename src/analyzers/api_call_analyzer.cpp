#include "analyzers/api_call_analyzer.h"

namespace probe {

std::vector<Finding> ApiCallAnalyzer::analyze(const AnalysisContext& ctx) const {
    std::vector<Finding> findings;
    auto indicators = ctx.profile.api_call_indicators();
    auto functions = ctx.nodes_of_type(NodeType::FUNCTION_DEF);

    for (const auto* fn : functions) {
        std::vector<std::string> matched_calls;

        for (const auto& call : fn->called_functions) {
            for (const auto& indicator : indicators) {
                // Match: "requests.get" starts with "requests",
                // or exact match like "fetch"
                if (call == indicator ||
                    (call.size() > indicator.size() &&
                     call.substr(0, indicator.size()) == indicator &&
                     call[indicator.size()] == '.')) {
                    matched_calls.push_back(call);
                    break;
                }
            }
        }

        if (matched_calls.empty()) continue;

        Finding f;
        f.file_path = fn->file_path;
        f.line_number = fn->start_line;
        f.function_name = fn->name;
        if (!fn->parent_class.empty()) {
            f.function_name = fn->parent_class + "." + fn->name;
        }
        f.type = FindingType::API_CALL;

        // Base confidence from number of distinct API calls
        f.confidence = std::min(0.5 + matched_calls.size() * 0.15, 0.95);

        f.reason = "Makes " + std::to_string(matched_calls.size())
                 + " external API call(s)";

        for (const auto& mc : matched_calls) {
            f.evidence.push_back("calls " + mc);
        }

        // Boost if function has decorator (likely a route handler)
        if (!fn->decorators.empty()) {
            f.confidence = std::min(f.confidence + 0.1, 0.99);
            f.evidence.push_back("has decorator: @" + fn->decorators[0]);
        }

        findings.push_back(std::move(f));
    }

    return findings;
}

} // namespace probe
