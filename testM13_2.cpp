#include <iostream>
#include "template/template_store.h"
#include "json.hpp"
using json = nlohmann::json;

static void test_m13_2_template_store_basic() {
    using namespace taskhub::tpl;

    // 1) create
    TaskTemplate t;
    t.templateId = "m13_2_t14";
    t.name = "demo";
    t.description = "desc";
    t.taskJsonTemplate = R"({
      "task": {
        "id": "{{task_id}}",
        "exec_type": "Shell",
        "exec_command": "{{cmd}}"
      }
    })"_json;

    t.schema.params = {
        {"task_id", ParamType::String, true,  {}},
        {"cmd",     ParamType::String, true,  {}}
    };

    bool ok = TemplateStore::instance().create(t);
    std::cout << "[M13.2] create: " << (ok ? "OK" : "FAIL") << "\n";
    assert(ok);

    // 2) get
    auto got = TemplateStore::instance().get("m13_2_t14");
    std::cout << "[M13.2] get: " << (got ? "OK" : "NULL") << "\n";
    assert(got.has_value());
    assert(got->templateId == "m13_2_t14");
    assert(got->schema.params.size() == 2);

    // 3) list (至少包含这一条)
    auto all = TemplateStore::instance().list();
    bool found = false;
    for (auto& x : all) {
        if (x.templateId == "m13_2_t14") { found = true; break; }
    }
    std::cout << "[M13.2] list: size=" << all.size() << " found=" << (found ? "yes" : "no") << "\n";
    assert(found);

    // 4) remove
    bool rm = TemplateStore::instance().remove("m13_2_t14");
    std::cout << "[M13.2] remove: " << (rm ? "OK" : "FAIL") << "\n";
    assert(rm);

    // 5) get again -> nullopt
    auto got2 = TemplateStore::instance().get("m13_2_t14");
    std::cout << "[M13.2] get after remove: " << (got2 ? "STILL EXISTS??" : "OK(null)") << "\n";
    assert(!got2.has_value());

    std::cout << "ALL M13.2 tests passed\n";
}