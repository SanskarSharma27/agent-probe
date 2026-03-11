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

struct Finding {
    std::string file_path;
    int line_number;
    std::string function_name;
    FindingType type;
    double confidence;       // 0.0 - 1.0
    std::string reason;
    std::vector<std::string> evidence;
};

} // namespace probe
