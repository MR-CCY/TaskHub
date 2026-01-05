#include <unordered_map>
#include <mutex>
#include <functional>
#include <atomic>
#include "task_config.h"
#include "task_result.h"

namespace taskhub::runner {

using LocalTaskFn = std::function<core::TaskResult(const core::TaskConfig&, std::atomic_bool*)>;

class LocalTaskRegistry {
public:
    static LocalTaskRegistry& instance();
    void registerTask(const std::string& key, LocalTaskFn fn);

    LocalTaskFn find(const std::string& key) const ;

private:
    LocalTaskRegistry() = default;
    LocalTaskRegistry(const LocalTaskRegistry&) = delete;
    LocalTaskRegistry& operator=(const LocalTaskRegistry&) = delete;

    mutable std::mutex mu_;
    std::unordered_map<std::string, LocalTaskFn> registry_;
};

} // namespace taskhub::runner
