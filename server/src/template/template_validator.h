#pragma once
#include <string>
#include <vector>
#include "template_types.h"

namespace taskhub::tpl {

struct ValidationError {
    std::string field;
    std::string message;
};

struct ValidationResult {
    bool ok{true};
    std::vector<ValidationError> errors;
    ParamMap resolved; // 合并 default 之后的最终参数
};

class TemplateValidator {
public:
    static ValidationResult validateAndResolve(const ParamSchema& schema,
                                               const ParamMap& input);

private:
    // TODO: 类型校验（String/Int/Bool/Json）
    static bool typeMatches_(ParamType t, const json& v);

    // TODO: 尝试做宽松转换？（例如 "123" -> int）你自己决定
};

} // namespace taskhub::tpl