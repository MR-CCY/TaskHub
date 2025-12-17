#include "template_store.h"
#include <mutex>  
namespace taskhub::tpl {
    TemplateStore &TemplateStore::instance()
    {
        // TODO: 在此处插入 return 语句
        static TemplateStore instance;
        return instance;
    }
    bool TemplateStore::create(const TaskTemplate &t)
    {
        // TODO: 在此处插入 return 语句
        std::lock_guard<std::shared_mutex> lock(_mu);
        _map.insert(std::make_pair(t.templateId, t));
        return true;
    }
    std::optional<TaskTemplate> TemplateStore::get(const std::string &templateId) const
    {
        std::lock_guard<std::shared_mutex> lock(_mu);  // 添加锁保护
        auto it = _map.find(templateId);
        if (it != _map.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    std::vector<TaskTemplate> TemplateStore::list() const
    {
        std::lock_guard<std::shared_mutex> lock(_mu);  // 添加锁保护
        std::vector<TaskTemplate> result;
        for (const auto& pair : _map) {
            result.push_back(pair.second);
        }
        return result;
    }
    bool TemplateStore::remove(const std::string &templateId)
    {
        std::lock_guard<std::shared_mutex> lock(_mu);  // 添加锁保护
        return _map.erase(templateId) > 0;
    }
}