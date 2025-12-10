#include <unordered_map>
#include <mutex>
#include "taskRunner.h"

namespace taskhub::runner {

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
