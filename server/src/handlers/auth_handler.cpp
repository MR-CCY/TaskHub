#include "auth_handler.h"
#include "auth/auth_manager.h"
#include "log/logger.h"
#include "json.hpp"
#include "core/http_response.h"

using nlohmann::json;
namespace taskhub {
    void AuthHandler::setup_routes(httplib::Server &server)
    {
        server.Post("/api/login", AuthHandler::login);
    }

    // Request: POST /api/login
    //   Headers: Content-Type: application/json
    //   Body: {"username":"admin","password":"123456"}
    // Response: {"code":0,"message":"ok","data":{"token":"<jwt-like>","username":"admin"}} on success;
    //           {"code":1004,"message":"invalid_username_or_password","data":null} on auth failure;
    //           4xx with code/message for bad JSON or missing fields.
    void taskhub::AuthHandler::login(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("POST /api/login");

        if (req.get_header_value("Content-Type").find("application/json") == std::string::npos) {
            resp::bad_request(res, "Content-Type must be application/json", 1001);
            return;
        }
    
        json body;
        try {
            body = json::parse(req.body);
        } catch (const std::exception& ex) {
            resp::bad_request(res, "Invalid JSON body", 1002);
            return;
        }
    
        if (!body.contains("username") || !body.contains("password") ||
            !body["username"].is_string() || !body["password"].is_string()) {
            resp::bad_request(res, "username and password required", 1003);
            return;
        }
    
        std::string username = body["username"].get<std::string>();
        std::string password = body["password"].get<std::string>();
    
        auto token_opt = AuthManager::instance().login(username, password);
        if (!token_opt) {
            resp::error(res, 1004, "invalid_username_or_password");
            return;
        }
    
        json data;
        data["token"] = *token_opt;
        data["username"] = username;
    
        resp::ok(res, data);
    }
}
