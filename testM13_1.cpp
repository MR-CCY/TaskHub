
#include <cassert>
#include <iostream>
#include <string>
#include <unordered_map>

#include "json.hpp"
using json = nlohmann::json;

// 你项目里的头文件（按你的真实路径改一下）
#include "template/task_template.h"
#include "template/template_renderer.h"
#include "runner/task_config.h"   // parseTaskConfigFromReq

// 让 R"(...)"_json 可用
using namespace nlohmann::json_literals;

static void test_ok_basic_render() {
    taskhub::tpl::TaskTemplate t;
    t.templateId = "t1";

    // ✅ 混合两种占位符：
    // 1) "{{xxx}}" —— string 替换（用于 JSON string value）
    // 2) { "$param": "xxx" } —— typed 注入（int/bool 等）
    t.taskJsonTemplate = R"({
      "task": {
        "id": "{{task_id}}",
        "exec_type": "Shell",
        "exec_command": "{{cmd}}",
        "timeout_ms": { "$param": "timeout_ms" },
        "capture_output": { "$param": "capture_output" },
        "exec_params": { "k": "{{k}}" },
        "metadata": { "m": "{{m}}" }
      }
    })"_json;

    // schema：你真实项目里如果是结构体数组/向量，按你定义填
    t.schema.params = {
        {"task_id",        taskhub::tpl::ParamType::String, true,  {}},
        {"cmd",            taskhub::tpl::ParamType::String, true,  {}},
        {"timeout_ms",     taskhub::tpl::ParamType::Int,    false, 0},
        {"capture_output", taskhub::tpl::ParamType::Bool,   false, true},
        {"k",              taskhub::tpl::ParamType::String, false, "v"},
        {"m",              taskhub::tpl::ParamType::String, false, ""}
    };

    taskhub::tpl::ParamMap p;
    p["task_id"]        = "log_test_1";
    p["cmd"]            = "echo hello";
    p["timeout_ms"]     = 5000;
    p["capture_output"] = true;
    // ✅ k/m 不提供，走默认值

    auto rr = taskhub::tpl::TemplateRenderer::render(t, p);
    assert(rr.ok);
    std::cout << "[OK] rendered json:\n" << rr.rendered.dump(2) << "\n";

    // ✅ 关键断言：默认值生效
    assert(rr.rendered["task"]["exec_params"]["k"] == "v");
    assert(rr.rendered["task"]["metadata"]["m"] == "");

    // ✅ 类型注入断言：timeout_ms 是数字，capture_output 是 bool
    assert(rr.rendered["task"]["timeout_ms"].is_number_integer());
    assert(rr.rendered["task"]["timeout_ms"] == 5000);

    assert(rr.rendered["task"]["capture_output"].is_boolean());
    assert(rr.rendered["task"]["capture_output"] == true);

    // ✅（建议做）把渲染结果再 parse 成 TaskConfig 做强校验
    auto cfg = taskhub::core::parseTaskConfigFromReq(rr.rendered);
    assert(cfg.id.value == "log_test_1");
    assert(taskhub::core::TaskExecTypetoString(cfg.execType) == "Shell");
    assert(cfg.execCommand == "echo hello");
    assert(cfg.timeout.count() == 5000);
    assert(cfg.captureOutput == true);
}

static void test_ok_param_dot_notation() {
    taskhub::tpl::TaskTemplate t;
    t.templateId = "t_param_dot";

    // ✅ 验证 param.xxx 的点号访问：
    // - 字符串占位符用 {{param.xxx}}
    // - typed 注入用 { "$param": "param.xxx" }
    t.taskJsonTemplate = R"({
      "task": {
        "id": "{{param.task_id}}",
        "exec_type": "Shell",
        "exec_command": "{{param.cmd}}",
        "timeout_ms": { "$param": "param.timeout_ms" },
        "capture_output": { "$param": "param.capture_output" },
        "exec_params": { "k": "{{param.k}}" },
        "metadata": { "m": "{{param.m}}" }
      }
    })"_json;

    // schema：依然按“扁平 key”校验（required / type / default）
    t.schema.params = {
        {"task_id",        taskhub::tpl::ParamType::String, true,  {}},
        {"cmd",            taskhub::tpl::ParamType::String, true,  {}},
        {"timeout_ms",     taskhub::tpl::ParamType::Int,    false, 0},
        {"capture_output", taskhub::tpl::ParamType::Bool,   false, true},
        {"k",              taskhub::tpl::ParamType::String, false, "v"},
        {"m",              taskhub::tpl::ParamType::String, false, ""}
    };

    taskhub::tpl::ParamMap p;
    // ✅ 仍提供扁平参数（让 validator 通过 + 默认值补全）
    p["task_id"]        = "log_test_param_dot";
    p["cmd"]            = "echo hello_param_dot";
    p["timeout_ms"]     = 1234;
    p["capture_output"] = false;
    // k/m 不提供，走默认值

    // ✅ 额外提供一个嵌套对象 param，专门给 {{param.xxx}} / $param: param.xxx 用
    // （不影响 schema 校验，因为 schema 里没有要求 param 这个顶层键）
    p["param"] = json{
        {"task_id", "log_test_param_dot"},
        {"cmd", "echo hello_param_dot"},
        {"timeout_ms", 1234},
        {"capture_output", false},
        {"k", "v"},
        {"m", ""}
    };

    auto rr = taskhub::tpl::TemplateRenderer::render(t, p);
    assert(rr.ok);
    std::cout << "[OK] rendered json (param.xxx):\n" << rr.rendered.dump(2) << "\n";

    // ✅ 基本断言
    assert(rr.rendered["task"]["id"] == "log_test_param_dot");
    assert(rr.rendered["task"]["exec_command"] == "echo hello_param_dot");

    // ✅ typed 注入断言
    assert(rr.rendered["task"]["timeout_ms"].is_number_integer());
    assert(rr.rendered["task"]["timeout_ms"] == 1234);

    assert(rr.rendered["task"]["capture_output"].is_boolean());
    assert(rr.rendered["task"]["capture_output"] == false);

    // ✅ 默认值（来自 schema）仍应生效
    assert(rr.rendered["task"]["exec_params"]["k"] == "v");
    assert(rr.rendered["task"]["metadata"]["m"] == "");
}

static void test_fail_missing_required() {
    taskhub::tpl::TaskTemplate t;
    t.templateId = "t1";
    t.taskJsonTemplate = R"({
      "task": { "id": "{{task_id}}", "exec_command": "{{cmd}}" }
    })"_json;

    t.schema.params = {
        {"task_id", taskhub::tpl::ParamType::String, true, {}},
        {"cmd",     taskhub::tpl::ParamType::String, true, {}}
    };

    taskhub::tpl::ParamMap p;
    p["task_id"] = "log_test_1";
    // ❌ 缺 cmd

    auto rr = taskhub::tpl::TemplateRenderer::render(t, p);
    assert(!rr.ok);
    assert(!rr.error.empty());
    std::cout << "[OK] missing required err: " << rr.error << "\n";
}

static void test_fail_type_mismatch() {
    taskhub::tpl::TaskTemplate t;
    t.templateId = "t1";
    t.taskJsonTemplate = R"({
      "task": {
        "id": "{{task_id}}",
        "timeout_ms": { "$param": "timeout_ms" }
      }
    })"_json;

    t.schema.params = {
        {"task_id",    taskhub::tpl::ParamType::String, true, {}},
        {"timeout_ms", taskhub::tpl::ParamType::Int,    true, {}}
    };

    taskhub::tpl::ParamMap p;
    p["task_id"] = "log_test_1";
    p["timeout_ms"] = "5000"; // ❌ 故意给 string，应该报错

    auto rr = taskhub::tpl::TemplateRenderer::render(t, p);
    assert(!rr.ok);
    assert(!rr.error.empty());
    std::cout << "[OK] type mismatch err: " << rr.error << "\n";
}

int main() {
    test_ok_basic_render();
    test_ok_param_dot_notation();
    test_fail_missing_required();
    test_fail_type_mismatch();
    std::cout << "\nALL M13.1 tests passed ✅\n";
    return 0;
}