#pragma once
#include <string>
#include "json.hpp"
#include "template_types.h"

namespace taskhub::tpl {

using json = nlohmann::json;

struct TaskTemplate {
    std::string templateId;
    std::string name;
    std::string description;

    // 渲染目标：必须是 A 格式：{ "task": {...} }
    json taskJsonTemplate;

    ParamSchema schema;
    json to_json() const {
        json j;
        j["template_id"] = templateId;
        j["name"] = name;
        j["description"] = description;
        j["task_json_template"] = taskJsonTemplate;
        // schema 序列化
        json schema_json = param_schema_to_json(schema);
        j["schema"] = schema_json;
        return j;
    }
    // version / createdAt 先不做（M13.2 + DB 时再加）
};

} // namespace taskhub::tpl