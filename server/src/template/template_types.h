#pragma once
#include <string>
#include <unordered_map>
#include <optional>
#include <variant>
#include <vector>
#include "json.hpp"

namespace taskhub::tpl {

using json = nlohmann::json;

// 支持的参数类型（M13.1 先够用）
enum class ParamType {
    String,
    Int,
    Bool,
    Json,      // 允许传 object/array，渲染时直接注入
};

// 一个参数定义
struct ParamDef {
    std::string name;
    ParamType type{ParamType::String};
    bool required{false};

    // defaultValue: 缺省参数（required=false 时可用）
    // 约定：类型要和 type 匹配
    json defaultValue; // TODO: 你决定 defaultValue 为空时的判定规则
};

// Schema：参数集合
struct ParamSchema {
    std::vector<ParamDef> params;

    // TODO: 提供快速查找
    const ParamDef* find(const std::string& name) const{
        for(const auto& param : params){
            if(param.name == name){
                return &param;
            }
        }
        return nullptr;
    };
};

// 渲染传入的参数（用户传的）
using ParamMap = std::unordered_map<std::string, json>;

} // namespace taskhub::tpl