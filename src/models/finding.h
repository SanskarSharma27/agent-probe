#pragma once

#include <string>
#include <vector>

namespace probe {

enum class FindingType {
    API_CALL,
    FAN_OUT,
    RETRY_PATTERN,
    CRUD_CLUSTER
};

inline std::string finding_type_str(FindingType t) {
    switch (t) {
        case FindingType::API_CALL:       return "API_CALL";
        case FindingType::FAN_OUT:        return "FAN_OUT";
        case FindingType::RETRY_PATTERN:  return "RETRY";
        case FindingType::CRUD_CLUSTER:   return "CRUD";
    }
    return "UNKNOWN";
}

struct Finding {
    std::string file_path;
    int line_number = 0;
    std::string function_name;
    FindingType type;
    double confidence = 0.0;    // 0.0 - 1.0
    std::string reason;
    std::vector<std::string> evidence;
};

} // namespace probe
