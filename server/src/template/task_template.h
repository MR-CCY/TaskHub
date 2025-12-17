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

    // version / createdAt 先不做（M13.2 + DB 时再加）
};

} // namespace taskhub::tpl