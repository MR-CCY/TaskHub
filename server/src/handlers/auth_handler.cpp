#include "auth_handler.h"
#include "core/auth_manager.h"
#include "log/logger.h"
#include "json.hpp"

using nlohmann::json;
namespace taskhub {
    static void make_json_response(httplib::Response& res,int code,const std::string& message,const json& data = json(nullptr))
    {
        json resp;
        resp["code"] = code;
        resp["message"] = message;
        resp["data"] = data;

        res.status = (code == 0 ? 200 : 400);
        res.set_content(resp.dump(), "application/json");
    }
    void taskhub::AuthHandler::login(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("POST /api/login");

        if (req.get_header_value("Content-Type").find("application/json") == std::string::npos) {
            make_json_response(res, 1001, "Content-Type must be application/json");
            return;
        }
    
        json body;
        try {
            body = json::parse(req.body);
        } catch (const std::exception& ex) {
            make_json_response(res, 1002, "Invalid JSON body");
            return;
        }
    
        if (!body.contains("username") || !body.contains("password") ||
            !body["username"].is_string() || !body["password"].is_string()) {
            make_json_response(res, 1003, "username and password required");
            return;
        }
    
        std::string username = body["username"].get<std::string>();
        std::string password = body["password"].get<std::string>();
    
        auto token_opt = AuthManager::instance().login(username, password);
        if (!token_opt) {
            make_json_response(res, 1004, "invalid_username_or_password");
            return;
        }
    
        json data;
        data["token"] = *token_opt;
        data["username"] = username;
    
        make_json_response(res, 0, "ok", data);
    }
}