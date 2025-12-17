#include "template_validator.h"

namespace taskhub::tpl {

bool TemplateValidator::typeMatches_(ParamType t, const json& v) {
    // TODO: 实现严格匹配（推荐先严格，减少歧义）
    // String -> v.is_string()
    // Int    -> v.is_number_integer()
    // Bool   -> v.is_boolean()
    // Json   -> v.is_object() || v.is_array() || v.is_null() || ...
    if(t == ParamType::String) 
    {
        return v.is_string();
    }
    else if(t == ParamType::Int) 
    {
        return v.is_number_integer();
    }
    else if(t == ParamType::Bool) 
    {
        return v.is_boolean();
    }
    else if(t == ParamType::Json) 
    {
        return v.is_object() || v.is_array() || v.is_null() || v.is_string() || v.is_number() || v.is_boolean();
    }
    return true;
}

/**
 * @brief 验证输入参数并解析默认值
 * 
 * 该函数根据给定的参数模式验证输入参数映射，并为缺失但有默认值的参数填充默认值。
 * 验证包括检查必需参数是否存在以及参数类型是否匹配。
 * 
 * @param schema 参数模式定义，包含参数的名称、类型、是否必需以及默认值等信息
 * @param input 输入的参数映射，需要被验证和解析
 * @return ValidationResult 验证结果，包含解析后的参数映射和错误信息列表
 */
ValidationResult TemplateValidator::validateAndResolve(const ParamSchema& schema,
                                                       const ParamMap& input) {
    ValidationResult r;

    // 初始化解析结果为输入参数
    r.resolved = input;

    // 遍历所有参数定义进行验证和解析
    for (const auto& def : schema.params) {
        auto it = input.find(def.name);
        if (it == input.end()) {
            // 处理参数不存在的情况
            if (def.required) {
                // 必需参数缺失，记录错误
                r.errors.push_back({def.name, "required"});
                continue;
            }
            // 非必需参数，如果有默认值则添加到解析结果中
            if (!def.defaultValue.is_null()) {
                r.resolved.emplace(def.name, def.defaultValue);
            }
            continue;
        }

        // 参数存在，检查类型是否匹配
        if (!typeMatches_(def.type, it->second)) {
            r.errors.push_back({def.name, "type mismatch"});
        }
    }

    // 根据是否有错误设置验证结果状态
    if (!r.errors.empty()) {
        r.ok = false;
    }

    return r;
}

} // namespace taskhub::tpl
