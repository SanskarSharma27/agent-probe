#include "analyzers/crud_analyzer.h"

#include <algorithm>
#include <set>

namespace probe {

// CRUD prefixes: create_user, get_user, update_user, delete_user
// Also handles: add_, remove_, fetch_, list_, find_, save_, destroy_
static const std::vector<std::pair<std::string, std::string>> PREFIX_PATTERNS = {
    {"create_", "create"}, {"add_", "create"}, {"insert_", "create"}, {"new_", "create"},
    {"get_", "read"},      {"fetch_", "read"}, {"find_", "read"},     {"list_", "read"},
    {"read_", "read"},     {"load_", "read"},  {"retrieve_", "read"},
    {"update_", "update"}, {"edit_", "update"}, {"modify_", "update"}, {"save_", "update"},
    {"delete_", "delete"}, {"remove_", "delete"}, {"destroy_", "delete"}, {"drop_", "delete"},
};

std::pair<std::string, std::string> CrudAnalyzer::extract_crud_pattern(const std::string& name) const {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (const auto& [prefix, operation] : PREFIX_PATTERNS) {
        if (lower.size() > prefix.size() && lower.substr(0, prefix.size()) == prefix) {
            std::string entity = lower.substr(prefix.size());
            // Strip trailing 's' for plurals (list_users → user)
            if (entity.size() > 1 && entity.back() == 's') {
                entity.pop_back();
            }
            return {entity, operation};
        }
    }

    return {"", ""};
}

std::vector<Finding> CrudAnalyzer::analyze(const AnalysisContext& ctx) const {
    std::vector<Finding> findings;
    auto functions = ctx.nodes_of_type(NodeType::FUNCTION_DEF);

    // Group functions by entity: entity → {operation → [function_names]}
    struct FnInfo {
        std::string full_name;
        std::string file_path;
        int line_number;
    };
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<FnInfo>>> entity_map;

    for (const auto* fn : functions) {
        auto [entity, operation] = extract_crud_pattern(fn->name);
        if (entity.empty()) continue;

        FnInfo info;
        info.full_name = fn->name;
        if (!fn->parent_class.empty()) {
            info.full_name = fn->parent_class + "." + fn->name;
        }
        info.file_path = fn->file_path;
        info.line_number = fn->start_line;

        entity_map[entity][operation].push_back(info);
    }

    // Generate findings for entities with 2+ CRUD operations
    for (const auto& [entity, ops] : entity_map) {
        if (ops.size() < 2) continue;

        // Collect all unique operations
        std::set<std::string> op_names;
        for (const auto& [op, fns] : ops) op_names.insert(op);

        // Use the first function's location as the finding location
        const auto& first_op = ops.begin()->second[0];

        Finding f;
        f.file_path = first_op.file_path;
        f.line_number = first_op.line_number;
        f.function_name = entity + " (CRUD cluster)";
        f.type = FindingType::CRUD_CLUSTER;

        // Score based on CRUD coverage: 2 ops = 0.5, 3 = 0.7, 4 = 0.9
        f.confidence = std::min(0.3 + ops.size() * 0.2, 0.95);

        f.reason = std::to_string(ops.size()) + " CRUD operations for entity '"
                 + entity + "'";

        for (const auto& [op, fns] : ops) {
            for (const auto& fn_info : fns) {
                f.evidence.push_back(op + ": " + fn_info.full_name);
            }
        }

        // Boost if functions share the same class (more cohesive)
        bool same_class = true;
        std::string first_file = first_op.file_path;
        for (const auto& [op, fns] : ops) {
            for (const auto& fn_info : fns) {
                if (fn_info.file_path != first_file) same_class = false;
            }
        }
        if (same_class && ops.size() >= 2) {
            f.confidence = std::min(f.confidence + 0.05, 0.95);
        }

        findings.push_back(std::move(f));
    }

    return findings;
}

} // namespace probe
