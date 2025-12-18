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
inline std::string ParamTypeToString(ParamType t){
    switch(t){
        case ParamType::String: return "string";
        case ParamType::Int: return "int";
        case ParamType::Bool: return "bool";
        case ParamType::Json: return "json";
        default: return "unknown";
    }
}
inline ParamType StringToParamType(const std::string& s){
    //转成小写再比较
    std::string str = s;
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    if(s == "string") return ParamType::String;
    if(s == "int") return ParamType::Int;
    if(s == "bool") return ParamType::Bool;
    if(s == "json") return ParamType::Json;
    return ParamType::String; // default
}

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
inline ParamSchema make_param_schema(const json& j){
    ParamSchema schema;
    if(j.contains("params") && j["params"].is_array()){
        for(const auto& jp : j["params"]){
            ParamDef pd;
            pd.name = jp.value("name", "");
            pd.type = StringToParamType(jp.value("type", "string"));
            pd.required = jp.value("required", false);
            pd.defaultValue = jp.value("defaultValue", json());
            schema.params.push_back(std::move(pd));
        }
    }
    return schema;
};
inline json param_schema_to_json(const ParamSchema& schema){
    json j = json::object();
    json params = json::array();
    for(const auto& pd : schema.params){
        json jp = json::object();
        jp["name"] = pd.name;
        jp["type"] = ParamTypeToString(pd.type);
        jp["required"] = pd.required;
        jp["defaultValue"] = pd.defaultValue;
        params.push_back(std::move(jp));
    }
    j["params"] = std::move(params);
    return j;
};

// 渲染传入的参数（用户传的）
using ParamMap = std::unordered_map<std::string, json>;

} // namespace taskhub::tpl