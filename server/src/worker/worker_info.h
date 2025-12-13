#pragma once
#include <chrono>
#include <string>
#include <vector>
namespace taskhub::worker {
    struct WorkerInfo {
        std::string id;                // worker-uuid
        std::string host="http://localhost";              // 例如 "172.16.1.10"
        int         port{8083};           // 例如 9001
    
        std::vector<std::string> queues;   // 支持哪些队列，例如 ["default", "io"]
        std::vector<std::string> labels;   // ["linux", "shell", "local"]
    
        int runningTasks{0};           // 当前正在执行的任务数
        std::chrono::steady_clock::time_point lastHeartbeat; // 最近心跳时间
    
        bool isAlive() const{
            using namespace std::chrono;
            // 简单策略：10 秒内有心跳就认为在线
            return (steady_clock::now() - lastHeartbeat) < seconds(10);
        };          // 根据 lastHeartbeat 判断是否失联
    };

}
