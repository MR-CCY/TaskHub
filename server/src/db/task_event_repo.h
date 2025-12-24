#pragma once

#include <string>
#include <vector>
#include "json.hpp"

namespace taskhub {

class TaskEventRepo {
public:
    static TaskEventRepo& instance();

    void insertFromJson(const nlohmann::json& eventJson);

    struct EventRow {
        long long id{0};
        std::string runId;
        std::string taskId;
        std::string type;
        std::string event;
        long long tsMs{0};
        nlohmann::json payload;
    };

    std::vector<EventRow> query(const std::string& runId,
                                const std::string& taskId,
                                const std::string& type,
                                const std::string& event,
                                long long startTs,
                                long long endTs,
                                int limit);

private:
    TaskEventRepo() = default;
};

} // namespace taskhub
