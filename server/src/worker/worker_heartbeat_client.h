#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
namespace taskhub::worker {
    // Worker-side heartbeat client running inside worker process.
    class WorkerHeartbeatClient {
    public:
        void start(std::string masterHost,
            int masterPort,
            std::string workerId,
            std::string workerHost,
            int workerPort,
            std::vector<std::string> queues,
            std::vector<std::string> labels,
            std::function<int()> getRunningTasks,
            std::chrono::milliseconds interval);

        void stop();

    private:
        void loop();

        std::function<int()> _getRunningTasks;

        std::chrono::milliseconds _interval{2000};
        std::thread _th;
        std::atomic_bool _stop{false};
        std::atomic_bool _registered{false};
        // 假设从配置读取
        std::string _masterHost = "127.0.0.1";
        int _masterPort = 8082;

        std::string _workerId = "worker-1";
        std::string _workerHost = "127.0.0.1";
        int _workerPort = 8083;

        std::vector<std::string> _queues = {"default"};
        std::vector<std::string> _labels = {"shell"};
        int _maxRunningTasks{1};
    };
}
