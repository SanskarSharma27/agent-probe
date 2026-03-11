#pragma once

#include <vector>
#include <string>
#include "models/finding.h"

namespace probe {

class Graph;

// Abstract base for all pattern analyzers.
// Each analyzer inspects the graph + AST data and produces findings.
class Analyzer {
public:
    virtual ~Analyzer() = default;

    virtual std::string name() const = 0;
    virtual std::vector<Finding> analyze(const Graph& graph) const = 0;
};

} // namespace probe
