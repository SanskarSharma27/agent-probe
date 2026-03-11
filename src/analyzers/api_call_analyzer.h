#pragma once

#include "analyzers/analyzer.h"

namespace probe {

// Detects functions that make external API/HTTP calls.
// Matches called_functions against the language profile's api_call_indicators
// (e.g., requests.get, httpx.post, aiohttp, boto3).
class ApiCallAnalyzer : public Analyzer {
public:
    std::string name() const override { return "api_call"; }
    std::vector<Finding> analyze(const AnalysisContext& ctx) const override;
};

} // namespace probe
