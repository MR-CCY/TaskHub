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
#include <vector>
#include "worker/worker_heartbeat_client.h"
#include "core/log_manager.h"
#include "core/log_sink_console.h"
#include "core/log_sink_file.h"
#include "core/log_rotation.h"
namespace taskhub {

    ServerApp::ServerApp() {
    }

    ServerApp::~ServerApp() {
    }

    // ... existing code ...
    
        /**
         * @brief 启动并运行服务器应用程序
         * 
         * 该方法负责初始化整个TaskHub服务器，包括配置加载、日志系统、数据库、HTTP服务等组件，
         * 并启动相关的工作线程和服务监听。
         * 
         * @return int 返回0表示成功启动服务器
         */
        int ServerApp::run() {
            // 1. 加载配置
            init_config();
    
            // 2. 初始化日志系统
            init_logger();
            Logger::info("===== TaskHub Server Starting =====");
    
            WorkerPool::instance()->start(1);
            
            // Logger::info("WorkerPool started with 4 workers");
            // 3. 创建 HTTP Server
            init_http_server();
    
            // 4. 注册路由
            setup_routes();
            Logger::info("Routes registered");

            // 5. 初始化数据库连接
            init_db();
    
            // 5. 初始化版本
            init_version();
    
            // 7. 监听 8090 端口启动 ws 服务（如果你希望任务日志通过 ws 实时推送，建议在 TaskRunner 之前启动）
            m_wsServer = std::make_unique<WsServer>("0.0.0.0", 8090);
            m_wsServer->start();
            Logger::info("WsServer started at 0.0.0.0:8090");
    
            // 8. 启动后台任务执行线程
            TaskRunner::instance().start();
            Logger::info("TaskRunner started");
    
            AuthManager::instance().init();
            Logger::info("AuthManager initialized");
    
             //9. 初始化 DAG 子系统
            init_dag();
            Logger::info("DagService initialized");
            //10. 启动定时任务调度器
            scheduler::CronScheduler::instance().start();
            Logger::info("CronScheduler started");
    
            //11. 初始化工作节点心跳客户端或启动工作节点注册清理器
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
            
            // 12. 启动监听（阻塞）
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

        // 约定：从配置读取日志文件路径（空串表示只打到控制台）
        // 你可以在 config.json 里加："log.path": "./logs/taskhub.log"
        const std::string logPath = cfg.get<std::string>("log.path", "");

        if (!logPath.empty()) {
            try {
                namespace fs = std::filesystem;
                fs::path p(logPath);
                if (p.has_parent_path()) {
                    fs::create_directories(p.parent_path());
                }
            } catch (...) {
                // 目录创建失败不致命：降级为控制台
            }
        }

        int maxRecords = cfg.get<int>("log.maxRecords", 2000);
        int rotateBytes = cfg.get<int>("log.rotateBytes", 10 * 1024 * 1024); // 10MB
        int maxFiles = cfg.get<int>("log.maxFiles", 5);
        bool compress = cfg.get<bool>("log.compress", false);
        
        taskhub::core::LogManager::instance().init(maxRecords);
        core::FileLogSink::Options opt;
        opt.path = logPath;
        opt.rotateBytes = rotateBytes;
        opt.maxFiles = maxFiles;
        opt.flushEachLine = compress;


        auto consoleSink = std::make_shared<taskhub::core::ConsoleLogSink>();
        auto fileSink    = std::make_shared<taskhub::core::FileLogSink>(opt);
    
        taskhub::core::LogManager::instance().setSinks({ consoleSink, fileSink });

        Logger::info(std::string("Logger initialized") + (logPath.empty() ? " (console-only)" : (" (file=" + logPath + ")")));
    }

    void ServerApp::init_http_server() {
        m_server=std::make_unique<httplib::Server>();

        // TODO:
        // - 启用 HTTPS 时在这里切换为 httplib::SSLServer
        // - 设置 Server 的超时、线程数等参数
// #if defined(DEBUG) || defined(_DEBUG)
//         // 调试时将线程池限制为 1，方便单步不被其他 worker 线程打断
//         m_server->new_task_queue = [] { return new httplib::ThreadPool(1); };
//         Logger::info("HTTP server thread pool set to 1 (debug build)");
// #endif
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

        // // 查找 migrations 目录：优先当前目录 ./migrations，其次 ../migrations，再到 build/bin/migrations、server/migrations
        // namespace fs = std::filesystem;
        // const std::vector<std::string> candidates = {
        //     "./migrations",
        //     "../migrations",
            
        //     "server/migrations"
        // };
        
        std::string migrations_dir="build/bin/migrations";
      
        // if (migrations_dir.empty()) {
        //     Logger::error("未找到 migrations 目录，尝试继续使用默认 ./migrations，迁移可能失败");
        //     migrations_dir = "./migrations";
        // } else {
        //     Logger::info("使用 migrations 目录: " + migrations_dir);
        // }

        taskhub::DbMigrator::migrate(db, migrations_dir);
    }

    void ServerApp::init_dag()
    {
        // 2. 用它构造 DagService
        dag::DagThreadPool::instance().start(4);
        taskhub::runner::registerDefaultExecutionStrategies();
    }
}
