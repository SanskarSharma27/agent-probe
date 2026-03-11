#pragma once

#include "analyzers/analyzer.h"

namespace probe {

// Detects retry/polling patterns: functions that contain loops
// with sleep/delay calls combined with API or function calls.
// These patterns indicate automated retry logic that agents
// could manage more intelligently (exponential backoff, circuit breaking).
//
// Detection strategy (AST-level, no LLM):
//   1. Function calls sleep/delay/wait (from called_functions)
//   2. Function also calls an external API or has "retry" in name/decorator
//   3. Boost if function name contains "retry", "poll", "sync", "wait"
class RetryAnalyzer : public Analyzer {
public:
    std::string name() const override { return "retry_pattern"; }
    std::vector<Finding> analyze(const AnalysisContext& ctx) const override;

private:
    bool has_sleep_call(const ASTNode& fn) const;
    bool has_retry_indicator(const ASTNode& fn) const;
};

} // namespace probe
