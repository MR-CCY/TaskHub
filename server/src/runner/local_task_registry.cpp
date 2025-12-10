#include "local_task_registry.h"
namespace taskhub::runner {
    LocalTaskRegistry &LocalTaskRegistry::instance()
    {
        static LocalTaskRegistry instance;
        return instance;
    }

    void LocalTaskRegistry::registerTask(const std::string &key, LocalTaskFn fn)
    {
        std::lock_guard<std::mutex> lock(mu_);
        registry_[key] = std::move(fn);
    }
    LocalTaskFn LocalTaskRegistry::find(const std::string &key) const
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = registry_.find(key);
        if (it == registry_.end()) {
            return nullptr;
        }
        return it->second;
    }
}
