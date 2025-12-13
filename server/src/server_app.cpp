//
// Created by 曹宸源 on 2025/11/11.
//

#include "server_app.h"
#include "core/config.h"
#include "core/logger.h"
#include "router.h"
#include "core/task_runner.h"
#include "core/auth_manager.h"
#include "db/db.h"
#include "db/migrator.h"
#include "dag/dag_thread_pool.h"
#include "execution/execution_registry.h"
#include "scheduler/cron_scheduler.h"
#include "dag/dag_types.h" 
#include "worker/worker_registry.h"   
#include <filesystem>
#include "worker/worker_heartbeat_client.h"
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

        WorkerPool::instance()->start(1);
        
        Logger::info("WorkerPool started with 4 workers");
        // 3. 创建 HTTP Server
        init_http_server();

        // 4. 注册路由
        setup_routes();
        init_db();

        // 5. 初始化数据库
        init_version();
        Logger::info("Routes registered");
        // 5. 启动后台任务执行线程
        TaskRunner::instance().start();
        Logger::info("TaskRunner started");
        AuthManager::instance().init();
        Logger::info("AuthManager initialized");

         // ★★★ 新增：初始化 DAG 子系统/主要是线程池
        init_dag();
        Logger::info("DagService initialized");

        scheduler::CronScheduler::instance().start();
        Logger::info("CronScheduler started");

        if(Config::instance().get("work.is_work", false)){
            m_host= Config::instance().get<std::string>("work.worker_host", "0.0.0.0");
            m_port= Config::instance().get("work.worker_port", 8083);
            std::string masterHost= Config::instance().get<std::string>("work.master_host", "127.0.0.1");
            int masterPort= Config::instance().get("work.master_port", 8082);
            std::string workerId= Config::instance().get<std::string>("work.worker_id", "worker-1");
            int intervalMs= Config::instance().get("work.heartbeat_interval_ms", 2000);
            std::vector<std::string> queues= Config::instance().get<std::vector<std::string>>("work.queues");
            std::vector<std::string> labels= Config::instance().get<std::vector<std::string>>("work.labels");
            auto test=[]() -> int {
                return 0;
            };

            g_heartbeat = std::make_unique<worker::WorkerHeartbeatClient>();
            g_heartbeat->start(masterHost,masterPort,workerId,m_host,m_port,queues,labels,test,std::chrono::milliseconds(intervalMs));
        
            Logger::info("WorkerHeartbeatClient started, interval=" +
                         std::to_string(intervalMs) + "ms");

        }else{
            worker::WorkerRegistry::instance().startSweeper(std::chrono::seconds(5),
                                                      std::chrono::seconds(60));
        }
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
        namespace fs = std::filesystem;
        const fs::path cwd = fs::current_path();

        // 优先加载生产环境配置
        bool loaded = cfg.load("/etc/taskhub/config.json");

        // 如果生产环境没有，就尝试其他几个可能的路径
        if (!loaded) {
            // 1) 当前工作目录
            loaded = cfg.load("config.json");
        }
        if (!loaded) {
            // 2) 构建输出目录（常见：build/bin/config.json）
            loaded = cfg.load((cwd / "build" / "bin" / "config.json").string());
        }
        if (!loaded) {
            // 3) 源码默认配置
            loaded = cfg.load((cwd / "server" / "config" / "default_config.json").string());
        }
        if (!loaded) {
            Logger::warn("No config file found in fallback paths, using built-in defaults");
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
#if defined(DEBUG) || defined(_DEBUG)
        // 调试时将线程池限制为 1，方便单步不被其他 worker 线程打断
        m_server->new_task_queue = [] { return new httplib::ThreadPool(1); };
        Logger::info("HTTP server thread pool set to 1 (debug build)");
#endif
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
                id           INTEGER PRIMARY KEY,
                name         TEXT NOT NULL,
                type         INTEGER NOT NULL,
                status       TEXT NOT NULL,
                params       TEXT,
                create_time  TEXT,
                update_time  TEXT,
                exit_code    INTEGER,
                last_output  TEXT,
                last_error   TEXT,
                timeout_sec  INTEGER NOT NULL DEFAULT 0,
                max_retries  INTEGER NOT NULL DEFAULT 0,
                retry_count  INTEGER NOT NULL DEFAULT 0,
                cancel_flag  INTEGER NOT NULL DEFAULT 0
            );
        )";

        if (! Db::instance().exec(sql)) {
            Logger::error("Create table tasks failed, exit.");
            std::exit(1);
        }
        Logger::info("Database initialized");
    }
    void ServerApp::init_version()
    {
        auto& cfg = Config::instance();
        std::string db_path = cfg.db_path();
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path.c_str(), &db);
        // 这里已有错误检查...

        std::string migrations_dir = "./migrations"; 
        // 或 Config 里搞一个 migrations_dir，可配置化也行

        taskhub::DbMigrator::migrate(db, migrations_dir);
    }
    void ServerApp::init_dag()
    {
        // 2. 用它构造 DagService
        dag::DagThreadPool::instance().start(4);
        taskhub::runner::registerDefaultExecutionStrategies();
    }
}
