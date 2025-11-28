//
// Created by 曹宸源 on 2025/11/11.
//

#include "server_app.h"
#include "core/config.h"
#include "core/logger.h"
#include "router.h"
#include "core/task_runner.h"
#include "core/auth_manager.h"
#include "core/db.h"
namespace taskhub {

    ServerApp::ServerApp() {
    }

    ServerApp::~ServerApp() {
    }

    int ServerApp::run() {
        // 1. 加载配置
        init_config();

        // 2. 初始化日志系统
        init_logger();
        Logger::info("===== TaskHub Server Starting =====");

        // 3. 创建 HTTP Server
        init_http_server();

        // 4. 注册路由
        setup_routes();
        init_db();
        Logger::info("Routes registered");
        // 5. 启动后台任务执行线程
        TaskRunner::instance().start();
        Logger::info("TaskRunner started");
        AuthManager::instance().init();
        Logger::info("AuthManager initialized");
        //7.监听8090 端口启动ws服务
        m_wsServer = std::make_unique<WsServer>("0.0.0.0", 8090);
        m_wsServer->start();
        Logger::info("WsServer started at 0.0.0.0:8090");
        // 6. 启动监听（阻塞）
        Logger::info("Listening at " + m_host + ":" + std::to_string(m_port));
        m_server->listen(m_host.c_str(), m_port);

        return 0;
    }

    void ServerApp::init_config() {
        auto& cfg = Config::instance();

        // 1. 尝试加载配置文件（后面你可以做真正的 JSON 解析）
        //    现在先允许失败，失败就用默认值

        // 优先加载生产环境配置
        bool loaded = cfg.load("/etc/taskhub/config.json");

        // 如果生产环境没有，就加载 build 目录里的
        if (!loaded) {
            cfg.load("config.json");   // 可执行文件旁边的
        }

        // 2. 环境变量覆盖（支持 Docker / 本地调试）
        cfg.load_from_env();

        // 3. 读出配置
        m_host = cfg.host();
        m_port = cfg.port();
        Logger::info(m_host);
        Logger::info(std::to_string(m_port));
    }

    void ServerApp::init_logger() {
        auto& cfg = Config::instance();
        // Logger::init(cfg.log_path());
        Logger::info("Logger initialized");
    }

    void ServerApp::init_http_server() {
        m_server=std::make_unique<httplib::Server>();

        // TODO:
        // - 启用 HTTPS 时在这里切换为 httplib::SSLServer
        // - 设置 Server 的超时、线程数等参数
    }
    void ServerApp::setup_routes() {
        Router::setup_routes(*m_server);
    }
    void ServerApp::init_db()
    {
        auto& cfg = Config::instance();
        bool ok = Db::instance().open(cfg.db_path());
        if (!ok) {
            Logger::error("Failed to open database, exiting");
            exit(1);
        }
         // 建表：tasks
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS tasks (
                id          INTEGER PRIMARY KEY,
                name        TEXT NOT NULL,
                type        INTEGER NOT NULL,
                status      TEXT NOT NULL,
                params      TEXT,
                create_time TEXT,
                update_time TEXT,
                exit_code   INTEGER,
                last_output TEXT,
                last_error  TEXT
            );
        )";

        if (! Db::instance().exec(sql)) {
            Logger::error("Create table tasks failed, exit.");
            std::exit(1);
        }
        Logger::info("Database initialized");
    }
}