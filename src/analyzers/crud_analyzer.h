#pragma once

#include "analyzers/analyzer.h"
#include <unordered_map>

namespace probe {

// Detects CRUD clusters: groups of functions that follow create/read/update/delete
// naming patterns for the same entity. These repetitive patterns are prime
// candidates for agent-based automation (auto-generate CRUD, manage via schema).
//
// Detection strategy:
//   1. Extract entity name from function names using CRUD prefix/suffix patterns
//   2. Group functions by entity
//   3. Score based on how many CRUD operations are present per entity
class CrudAnalyzer : public Analyzer {
public:
    std::string name() const override { return "crud_cluster"; }
    std::vector<Finding> analyze(const AnalysisContext& ctx) const override;

private:
    // Try to extract entity name and CRUD operation from a function name.
    // Returns {entity, operation} or {"", ""} if no pattern matches.
    std::pair<std::string, std::string> extract_crud_pattern(const std::string& name) const;
};

} // namespace probe
