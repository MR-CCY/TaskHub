#include <iostream>
#include <algorithm>
#include <cctype>
#include "template_renderer.h"
#include "runner/task_config.h"   // 你现有的 parseTaskConfigFromReq

namespace taskhub::tpl {

/**
 * @brief 渲染任务模板，根据输入参数生成最终的任务配置。
 *
 * 该函数首先校验并补全输入参数，然后使用模板渲染引擎将任务 JSON 模板进行渲染，
 * 最后解析渲染后的 JSON 并做基本合法性检查（如 exec_type 合法性、任务 ID 是否存在等）。
 *
 * @param t 任务模板对象，包含 schema 和 taskJsonTemplate。
 * @param input 用户传入的原始参数映射表。
 * @return 返回一个 RenderResult 对象，其中包含渲染结果或错误信息。
 */
RenderResult TemplateRenderer::render(const TaskTemplate& t,
                                      const ParamMap& input) {
    RenderResult rr;

    // 1) 校验/补全参数：确保所有必需字段都存在且类型正确，并完成默认值填充
    auto vr = TemplateValidator::validateAndResolve(t.schema, input);
    if (!vr.ok) {
        rr.ok = false;
    
        // ✅ 把 vr.errors 拼成 rr.error
        // 期望 vr.errors 的元素形如 { name, reason }
        std::string msg;
        for (size_t i = 0; i < vr.errors.size(); ++i) {
            const auto& e = vr.errors[i];
            if (i) msg += "; ";
            msg += e.field;
            msg += ": ";
            msg += e.message;
        }
        rr.error = msg.empty() ? "validateAndResolve failed" : msg;
    
        return rr;
    }

    // 2) 使用已解析和补全的参数渲染 JSON 模板
    std::string err;
    rr.rendered = renderNode_(t.taskJsonTemplate, vr.resolved, err);
    if (!err.empty()) {
        rr.ok = false;
        rr.error = err;
        return rr;
    }

    // ✅ DAG template: contains tasks array, only render JSON, skip TaskConfig validation
    if (rr.rendered.contains("tasks") && rr.rendered["tasks"].is_array()) {
        return rr;
    }

    // 获取实际的 task 节点用于后续处理；如果不存在则直接使用整个渲染结果
    const json& jt = (rr.rendered.contains("task") && rr.rendered["task"].is_object())
                         ? rr.rendered["task"]
                         : rr.rendered;

    std::string execTypeStr;
    const bool hasExecType = jt.contains("exec_type");

    // 若存在 exec_type 字段，则验证其是否为字符串类型
    if (hasExecType) {
        if (!jt["exec_type"].is_string()) {
            rr.ok = false;
            rr.error = "exec_type must be string";
            return rr;
        }
        execTypeStr = jt["exec_type"].get<std::string>();
    }
    // 解析任务配置结构体，从渲染后的 JSON 中提取核心配置项
    core::TaskConfig cfg;
    try {
        cfg = core::parseTaskConfigFromReq(rr.rendered);
    } catch (const std::exception& e) {
        rr.ok = false;
        rr.error = std::string("parseTaskConfigFromReq failed: ") + e.what();
        return rr;
    }

    // 验证任务 ID 是否存在
    if (cfg.id.value.empty()) {
        rr.ok = false;
        rr.error = "task id is required";
        return rr;
    }

    // 若提供了 exec_type，则进一步验证其是否属于合法枚举范围
    if (hasExecType) {
        std::string t = execTypeStr;
        // 将字符串t中的所有字符转换为小写
        // 使用std::transform算法遍历字符串t的每个字符，应用lambda表达式将每个字符转为小写
        // 参数说明：
        //   t.begin() - 输入序列的起始迭代器
        //   t.end() - 输入序列的结束迭代器
        //   t.begin() - 输出序列的起始迭代器（原地修改）
        //   lambda表达式 - 转换函数，将unsigned char转换为小写字符
        // 返回值：无显式返回值，但会修改原字符串t的内容
        std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (t != "local" && t != "remote" && t != "script" && t != "httpcall" && t != "http_call" && t != "http" && t != "shell") {
            rr.ok = false;
            rr.error = "invalid exec_type: " + execTypeStr;
            return rr;
        }
    }

    return rr;
}

/**
 * @brief 渲染JSON节点，递归处理对象、数组、字符串等类型
 * 
 * @param node 待渲染的JSON节点
 * @param resolved 已解析的参数映射表
 * @param err 错误信息输出参数，如果渲染过程中出现错误则填充错误信息
 * @return json 渲染后的JSON节点，如果出错则返回空JSON对象
 */
json TemplateRenderer::renderNode_(const json& node, const ParamMap& resolved, std::string& err) {
    // 如果已有错误，直接返回空JSON
    if (!err.empty()) {
        return json();
    }

    // 处理对象类型的节点
    if (node.is_object()) {
        std::string paramName;
        // 检查是否为参数注入节点
        if (isParamInject_(node, &paramName)) {
            json value;
            if (!resolveParamPath_(resolved, paramName, value, err)) {
                return json();
            }
            // ✅ typed inject: keep JSON type (bool/int/number/object/array/string)
            return value;
        }

        // 递归渲染对象的所有键值对
        json obj = json::object();
        for (const auto& kv : node.items()) {
            obj[kv.key()] = renderNode_(kv.value(), resolved, err);
            if (!err.empty()) {
                return json();
            }
        }
        return obj;
    }

    // 处理数组类型的节点
    if (node.is_array()) {
        json arr = json::array();
        // 递归渲染数组中的每个元素
        for (const auto& item : node) {
            arr.push_back(renderNode_(item, resolved, err));
            if (!err.empty()) {
                return json();
            }
        }
        return arr;
    }

    // 处理字符串类型的节点
    if (node.is_string()) {
        return renderString_(node.get_ref<const std::string&>(), resolved, err);
    }

    // 其他基本类型直接返回原值
    return node;
}

/**
 * @brief 检查JSON节点是否为参数注入标记，并提取参数名
 * 
 * 该函数用于判断给定的JSON节点是否符合参数注入的格式要求，
 * 即节点为对象类型、只包含一个键值对，且键为"$param"，值为字符串类型。
 * 如果符合条件，则将参数名存储到输出参数中。
 * 
 * @param node 待检查的JSON节点
 * @param outParamName 输出参数，用于存储提取到的参数名（可为空指针）
 * @return bool 如果节点符合参数注入格式则返回true，否则返回false
 */
bool TemplateRenderer::isParamInject_(const json& node, std::string* outParamName) {
    // 检查节点是否为对象类型且仅包含一个键值对
    if (!node.is_object() || node.size() != 1) {
        return false;
    }

    // 查找"$param"键并验证其值是否为字符串类型
    auto it = node.find("$param");
    if (it == node.end() || !it->is_string()) {
        return false;
    }

    // 如果输出参数指针有效，则保存参数名
    if (outParamName) {
        *outParamName = it->get<std::string>();
    }
    return true;
}

/**
 * @brief 解析并获取参数路径对应的值
 * 
 * 该函数用于在已解析的参数映射中查找指定路径的参数值。
 * 路径格式为 "param.xxx.yyy" 形式，通过点号分隔层级结构。
 * 
 * @param resolved 已解析的参数映射表
 * @param path 参数路径，格式如 "param.xxx.yyy"
 * @param out 输出参数，存储找到的参数值
 * @param err 错误信息输出参数，当返回false时包含错误描述
 * @return bool 成功找到参数返回true，否则返回false
 */
bool TemplateRenderer::resolveParamPath_(const ParamMap &resolved, const std::string &path, json &out, std::string &err)
{
      // 拆分 param.xxx.yyy
      std::vector<std::string> parts;
      size_t start = 0;
      while (true) {
          size_t dot = path.find('.', start);
          if (dot == std::string::npos) {
              parts.push_back(path.substr(start));
              break;
          }
          parts.push_back(path.substr(start, dot - start));
          start = dot + 1;
      }
  
      if (parts.empty()) {
          err = "empty param path";
          return false;
      }
  
      // 1️⃣ 先找一级 param
      auto it = resolved.find(parts[0]);
      if (it == resolved.end()) {
          err = "missing param: " + parts[0];
          return false;
      }
  
      json cur = it->second;
  
      // 2️⃣ 逐级向下找
      for (size_t i = 1; i < parts.size(); ++i) {
          if (!cur.is_object()) {
              err = "param is not object: " + parts[i - 1];
              return false;
          }
          if (!cur.contains(parts[i])) {
              err = "missing param field: " + parts[i];
              return false;
          }
          cur = cur[parts[i]];
      }
  
      out = cur;
      return true;
}

/**
 * @brief 渲染模板字符串，将字符串中的占位符替换为对应的参数值
 * 
 * 该函数处理形如 "{{key}}" 的占位符，将其替换为resolved映射中对应key的值。
 * 如果遇到错误，会设置err参数并返回空字符串。
 * 
 * @param s 待渲染的模板字符串
 * @param resolved 参数映射表，用于查找占位符对应的值
 * @param err 错误信息输出参数，如果发生错误则会被设置为相应的错误信息
 * @return 渲染后的字符串，如果发生错误则返回空字符串
 */
std::string TemplateRenderer::renderString_(const std::string& s, const ParamMap& resolved, std::string& err) {
    // 如果已有错误或输入字符串为空，直接返回原字符串
    if (!err.empty() || s.empty()) {
        return s;
    }

    std::string out;
    out.reserve(s.size());

    size_t pos = 0;
    // 遍历字符串，查找并处理所有占位符
    while (pos < s.size()) {
        // 查找下一个占位符开始标记 "{{"
        size_t start = s.find("{{", pos);
        if (start == std::string::npos) {
            // 没有找到更多占位符，将剩余部分追加到结果中
            out.append(s, pos, std::string::npos);
            break;
        }

        // 将占位符之前的普通文本追加到结果中
        out.append(s, pos, start - pos);
        
        // 查找占位符结束标记 "}}"
        size_t end = s.find("}}", start + 2);
        if (end == std::string::npos) {
            // 没有找到结束标记，将剩余部分追加到结果中
            out.append(s, start, std::string::npos);
            break;
        }

        // 提取占位符中的键名
        const size_t nameStart = start + 2;
        std::string key = s.substr(nameStart, end - nameStart);
        // trim spaces inside {{  }}
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.front()))) key.erase(key.begin());
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) key.pop_back();

        // 支持两种：{{key}}（旧） 和 {{param.xxx.yyy}}（新）
        json v;
        if (key.find('.') != std::string::npos) {
            if (!resolveParamPath_(resolved, key, v, err)) {
                return std::string();
            }
        } else {
            auto it = resolved.find(key);
            if (it == resolved.end()) {
                err = "missing param: " + key;
                return std::string();
            }
            v = it->second;
        }

        // 将参数值转换为字符串并追加到结果中
        out += paramToString_(v);
        // 更新位置到占位符之后
        pos = end + 2;
    }

    return out;
}

std::string TemplateRenderer::paramToString_(const json& v) {
    if (v.is_string()) {
        return v.get_ref<const std::string&>();
    }
    if (v.is_boolean()) {
        return v.get<bool>() ? "true" : "false";
    }
    if (v.is_number_integer()) {
        return std::to_string(v.get<long long>());
    }
    if (v.is_number_unsigned()) {
        return std::to_string(v.get<unsigned long long>());
    }
    if (v.is_number_float()) {
        return std::to_string(v.get<double>());
    }
    return v.dump();
}

} // namespace taskhub::tpl
