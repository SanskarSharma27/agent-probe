#pragma once

#include <vector>
#include <string>
#include "models/finding.h"
#include "analyzers/analysis_context.h"

namespace probe {

// Abstract base for all pattern analyzers.
// Each analyzer inspects the analysis context and produces findings.
class Analyzer {
public:
    virtual ~Analyzer() = default;

    virtual std::string name() const = 0;
    virtual std::vector<Finding> analyze(const AnalysisContext& ctx) const = 0;
};

} // namespace probe
