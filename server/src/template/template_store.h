#pragma once
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include "task_template.h"

namespace taskhub::tpl {

class TemplateStore {
public:
    static TemplateStore& instance();

    // CRUD（M13.1 先做最小：create/get/list/delete）
    bool create(const TaskTemplate& t);          // TODO: id 冲突处理
    std::optional<TaskTemplate> get(const std::string& templateId) const;
    std::vector<TaskTemplate> list() const;
    bool remove(const std::string& templateId);

private:
    TemplateStore() = default;

    mutable std::shared_mutex _mu;
    std::unordered_map<std::string, TaskTemplate> _map;
};

} // namespace taskhub::tpl