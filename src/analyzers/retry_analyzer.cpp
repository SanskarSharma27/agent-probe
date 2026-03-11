#include "analyzers/retry_analyzer.h"

#include <algorithm>

namespace probe {

static const std::vector<std::string> SLEEP_PATTERNS = {
    "time.sleep", "sleep", "asyncio.sleep", "await asyncio.sleep",
    "threading.Event().wait", "delay"
};

static const std::vector<std::string> RETRY_NAME_PATTERNS = {
    "retry", "poll", "sync", "wait", "backoff", "attempt"
};

static const std::vector<std::string> RETRY_DECORATORS = {
    "retry", "backoff", "retrying", "tenacity"
};

bool RetryAnalyzer::has_sleep_call(const ASTNode& fn) const {
    for (const auto& call : fn.called_functions) {
        for (const auto& pattern : SLEEP_PATTERNS) {
            if (call == pattern || call.find(pattern) != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}

bool RetryAnalyzer::has_retry_indicator(const ASTNode& fn) const {
    // Check function name
    std::string lower_name = fn.name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

    for (const auto& pattern : RETRY_NAME_PATTERNS) {
        if (lower_name.find(pattern) != std::string::npos) return true;
    }

    // Check decorators
    for (const auto& dec : fn.decorators) {
        std::string lower_dec = dec;
        std::transform(lower_dec.begin(), lower_dec.end(), lower_dec.begin(), ::tolower);
        for (const auto& pattern : RETRY_DECORATORS) {
            if (lower_dec.find(pattern) != std::string::npos) return true;
        }
    }

    return false;
}

std::vector<Finding> RetryAnalyzer::analyze(const AnalysisContext& ctx) const {
    std::vector<Finding> findings;
    auto functions = ctx.nodes_of_type(NodeType::FUNCTION_DEF);
    auto indicators = ctx.profile.api_call_indicators();

    for (const auto* fn : functions) {
        bool sleep_found = has_sleep_call(*fn);
        bool retry_name = has_retry_indicator(*fn);

        // Check if function also makes API calls
        bool has_api_call = false;
        std::string api_call_name;
        for (const auto& call : fn->called_functions) {
            for (const auto& indicator : indicators) {
                if (call == indicator ||
                    (call.size() > indicator.size() &&
                     call.substr(0, indicator.size()) == indicator &&
                     call[indicator.size()] == '.')) {
                    has_api_call = true;
                    api_call_name = call;
                    break;
                }
            }
            if (has_api_call) break;
        }

        // Must have at least one strong signal
        if (!sleep_found && !retry_name) continue;

        // Need either sleep+API or explicit retry indicator
        if (!sleep_found && !has_api_call) continue;

        Finding f;
        f.file_path = fn->file_path;
        f.line_number = fn->start_line;
        f.function_name = fn->name;
        if (!fn->parent_class.empty()) {
            f.function_name = fn->parent_class + "." + fn->name;
        }
        f.type = FindingType::RETRY_PATTERN;

        // Score based on combined signals
        double score = 0.3;
        if (sleep_found) {
            score += 0.25;
            f.evidence.push_back("contains sleep/delay call");
        }
        if (has_api_call) {
            score += 0.2;
            f.evidence.push_back("calls API: " + api_call_name);
        }
        if (retry_name) {
            score += 0.2;
            f.evidence.push_back("name suggests retry/polling pattern");
        }

        f.confidence = std::min(score, 0.95);
        f.reason = "Retry/polling pattern detected";

        findings.push_back(std::move(f));
    }

    return findings;
}

} // namespace probe
