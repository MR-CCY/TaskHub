#pragma once

#include <string>
#include "json.hpp"

namespace taskhub {

class DagRunRepo {
public:
    static DagRunRepo& instance();

    void insertRun(const std::string& runId,
                   const nlohmann::json& body,
                   const std::string& source,
                   const std::string& workflowJson);

    void finishRun(const std::string& runId,
                   int status,
                   long long endTsMs,
                   int total,
                   int ok,
                   int failed,
                   int skipped,
                   const std::string& message);

    struct DagRunRow {
        std::string runId;
        std::string name;
        std::string source;
        int status{0};
        long long startTsMs{0};
        long long endTsMs{0};
        int total{0};
        int successCount{0};
        int failedCount{0};
        int skippedCount{0};
        std::string message;
        std::string workflowJson;
        std::string dagJson;
    };

    std::vector<DagRunRow> query(const std::string& runId,
                                 const std::string& nameLike,
                                 long long startTs,
                                 long long endTs,
                                 int limit);

private:
    DagRunRepo() = default;
};

} // namespace taskhub
