#pragma once

#include <vector>
#include "models/finding.h"

namespace probe {

// Combines raw analyzer signals into a normalized confidence score.
class Scorer {
public:
    // Re-score findings using graph-derived metrics and configurable weights
    std::vector<Finding> score(const std::vector<Finding>& raw_findings) const;

    void set_weight(FindingType type, double weight);

private:
    double base_weight(FindingType type) const;
};

} // namespace probe
