#pragma once
#include <string>
#include "json.hpp"
#include "task_template.h"
#include "template_validator.h"

namespace taskhub::tpl {

using json = nlohmann::json;

struct RenderResult {
    bool ok{true};
    std::string error;
    json rendered;     // 结果仍为 A 格式 { "task": {...} }
    json to_json() const {
        json j;
        j["ok"] = ok;
        j["error"] = error;
        j["rendered"] = rendered;
        return j;
    };
};

class TemplateRenderer {
public:
    // 输入：模板 + 参数
    static RenderResult render(const TaskTemplate& t, const ParamMap& input);

private:
    // TODO: 递归渲染 json（object/array/string）
    static json renderNode_(const json& node, const ParamMap& resolved, std::string& err);

    // TODO: 替换 string 中的 {{var}}
    static std::string renderString_(const std::string& s, const ParamMap& resolved, std::string& err);

    // TODO: 获取参数并转字符串（用于字符串替换）
    static std::string paramToString_(const json& v);

    // TODO: 判断是否为注入节点：{"$param":"xxx"}
    static bool isParamInject_(const json& node, std::string* outParamName);
    static bool resolveParamPath_(const ParamMap& resolved,const std::string& path,json& out,std::string& err);
};

} // namespace taskhub::tpl