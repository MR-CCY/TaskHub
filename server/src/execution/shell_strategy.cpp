#include "shell_strategy.h"
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

namespace taskhub::runner {

/**
 * @brief 设置文件描述符为非阻塞模式
 * 
 * 该函数通过修改文件描述符的标志位，将其设置为非阻塞I/O模式。
 * 在非阻塞模式下，I/O操作会立即返回，而不会等待数据就绪或操作完成。
 * 
 * @param fd 要设置为非阻塞模式的文件描述符
 */
static void set_nonblocking(int fd) {
    // 获取当前文件描述符的标志位
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) return;
    
    // 设置非阻塞标志位
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * @brief 从文件描述符中读取所有可用数据并追加到输出字符串中
 * 
 * 该函数会持续从指定的文件描述符读取数据，直到遇到EOF或读取错误。
 * 读取的数据会被追加到out字符串中。当遇到EOF时，会关闭文件描述符并设置openFlag为false。
 * 当遇到EAGAIN或EWOULDBLOCK错误时，函数会返回但保持文件描述符打开。
 * 其他读取错误会导致文件描述符被关闭并设置openFlag为false。
 * 
 * @param fd 要读取的文件描述符
 * @param out 用于存储读取数据的字符串引用，新数据会被追加到该字符串末尾
 * @param openFlag 文件描述符状态标志的引用，当文件描述符被关闭时设置为false
 */
static void drain_fd(int fd, std::string& out, bool& openFlag) {
    char buf[4096];
    while (true) {
        const ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n > 0) {
            out.append(buf, static_cast<size_t>(n));
            continue;
        }
        if (n == 0) {
            // EOF: 读取完毕，关闭文件描述符
            openFlag = false;
            ::close(fd);
            return;
        }
        // n < 0
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 非阻塞模式下没有更多数据可读，暂时返回
            return;
        }
        // 其他读取错误，关闭文件描述符并标记为关闭状态
        openFlag = false;
        ::close(fd);
        return;
    }
}

static void emit_lines_to_log_buffer(const taskhub::core::TaskId& taskId, bool isStdout, const std::string& text) {
    if (text.empty()) return;

    std::string line;
    line.reserve(256);
    for (char c : text) {
        if (c == '\n') {
            // push a line (skip pure empty line if you want; here we allow empty lines)
            if (isStdout) {
                taskhub::core::LogManager::instance().stdoutLine(taskId, line);
            } else {
                taskhub::core::LogManager::instance().stderrLine(taskId, line);
            }
            line.clear();
        } else if (c != '\r') {
            line.push_back(c);
        }
    }

    // trailing line without newline
    if (!line.empty()) {
        if (isStdout) {
            taskhub::core::LogManager::instance().stdoutLine(taskId, line);
        } else {
            taskhub::core::LogManager::instance().stderrLine(taskId, line);
        }
    }
}


/**
 * @brief 执行一个 Shell 命令任务。
 *
 * 此函数通过 fork/exec 方式启动一个新的子进程来执行指定的 Shell 命令，
 * 并捕获其标准输出和错误输出。同时监控任务是否被取消或超时，并根据结果设置 TaskResult。
 *
 * @param cfg 包含执行所需配置的任务配置对象，包括命令、是否捕获输出等信息。
 * @param cancelFlag 用于指示任务是否应被取消的原子布尔标志指针，可为空。
 * @param deadline 表示任务允许运行到的时间点，超过该时间则认为是超时。
 * @return core::TaskResult 返回任务执行的结果，包括状态、消息、退出码、输出及耗时等信息。
 */
core::TaskResult ShellExecutionStrategy::execute(const core::TaskConfig &cfg,
                                                std::atomic_bool *cancelFlag,
                                                Deadline deadline)
{
    core::TaskResult r;
    const auto start = SteadyClock::now();

    // 1) 基本校验：检查命令是否为空以及任务是否已被取消
    if (cfg.execCommand.empty()) {
        r.status  = core::TaskStatus::Failed;
        r.message = "TaskRunner：空Shell命令";
        // 失败也发一条事件（便于定位）
        core::emitEvent(cfg, LogLevel::Error, "Shell rejected: empty exec_command", 0);
        return r;
    }

    if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
        r.status  = core::TaskStatus::Canceled;
        r.message = "TaskRunner：在Shell执行前取消";
        core::emitEvent(cfg, LogLevel::Warn, "Shell canceled before start", 0);
        return r;
    }

    // start event
    core::emitEvent(cfg, LogLevel::Info, std::string("Shell start: ") + cfg.execCommand, 0);

    // 2) 创建 stdout / stderr 管道以捕获子进程输出
    int outPipe[2]{-1, -1};
    int errPipe[2]{-1, -1};
    if (::pipe(outPipe) != 0 || ::pipe(errPipe) != 0) {
        r.status  = core::TaskStatus::Failed;
        r.message = "TaskRunner：pipe() 创建失败";
        return r;
    }

    // 3) 使用 fork 启动子进程执行命令
    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(outPipe[0]); ::close(outPipe[1]);
        ::close(errPipe[0]); ::close(errPipe[1]);
        r.status  = core::TaskStatus::Failed;
        r.message = "TaskRunner：fork() 失败";
        return r;
    }

    if (pid == 0) {
        // ---- child ----
        // 设置新的进程组 ID，方便后续统一终止整个进程组
        ::setpgid(0, 0);

        // 将 stdout 和 stderr 重定向至管道写端
        ::dup2(outPipe[1], STDOUT_FILENO);
        ::dup2(errPipe[1], STDERR_FILENO);

        // 关闭不需要的文件描述符
        ::close(outPipe[0]); ::close(outPipe[1]);
        ::close(errPipe[0]); ::close(errPipe[1]);

        // 调用 /bin/sh 执行用户提供的命令字符串
        const char* sh = "/bin/sh";
        ::execl(sh, sh, "-c", cfg.execCommand.c_str(), (char*)nullptr);

        // 如果 exec 失败，则直接退出并返回错误码 127
        _exit(127);
    }

    // ---- parent ----
    // 设置子进程所属的进程组，防止竞争条件
    ::setpgid(pid, pid);

    // 关闭父进程中不需要的写端，并设为非阻塞模式以便异步读取
    ::close(outPipe[1]);
    ::close(errPipe[1]);
    set_nonblocking(outPipe[0]);
    set_nonblocking(errPipe[0]);

    bool outOpen = true;
    bool errOpen = true;
    bool childExited = false;
    int  childStatus = 0;

    std::string stdoutBuf;
    std::string stderrBuf;

    bool timedOut = false;
    bool canceled = false;

    // 定义 lambda 函数用于向整个进程组发送信号
    auto kill_group = [&](int sig) {
        ::killpg(pid, sig);
    };

    // 检查当前是否已经超时
    auto check_deadline = [&]() {
        if (!timedOut && deadline != Deadline::max()) {
            if (SteadyClock::now() >= deadline) {
                timedOut = true;
            }
        }
    };

    // 4) 主循环持续读取管道数据直到子进程结束且所有输出都被消费完毕
    while (outOpen || errOpen || !childExited) {
        // 4.1 检查是否有外部请求取消任务或发生超时
        if (!canceled && cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
            canceled = true;
        }
        check_deadline();

        // 4.2 当需要终止时，首先尝试优雅地关闭（SIGTERM），若无效再强制杀死（SIGKILL）
        if ((timedOut || canceled) && !childExited) {
            kill_group(SIGTERM);
            for (int i = 0; i < 20; ++i) {
                const int w = ::waitpid(pid, &childStatus, WNOHANG);
                if (w == pid) {
                    childExited = true;
                    break;
                }
                ::usleep(10 * 1000); // 等待 10ms
            }
            if (!childExited) {
                kill_group(SIGKILL);
            }
        }

        // 4.3 使用 poll 监听管道是否有数据可读，避免忙等
        pollfd fds[2];
        int nfds = 0;
        if (outOpen) fds[nfds++] = pollfd{outPipe[0], POLLIN, 0};
        if (errOpen) fds[nfds++] = pollfd{errPipe[0], POLLIN, 0};

        if (nfds > 0) {
            ::poll(fds, nfds, 50); // 最多等待 50ms
        } else {
            ::usleep(10 * 1000);   // 若无监听目标，则短暂休眠
        }

        // 4.4 从管道中尽可能多地读取数据
        if (outOpen) drain_fd(outPipe[0], stdoutBuf, outOpen);
        if (errOpen) drain_fd(errPipe[0], stderrBuf, errOpen);

        // 4.5 查询子进程是否已完成执行
        if (!childExited) {
            const int w = ::waitpid(pid, &childStatus, WNOHANG);
            if (w == pid) {
                childExited = true;
            }
        }
    }

    // 5) 根据 waitpid 的返回解析实际退出码
    int exitCode = -1;
    if (WIFEXITED(childStatus)) {
        exitCode = WEXITSTATUS(childStatus);
    } else if (WIFSIGNALED(childStatus)) {
        exitCode = 128 + WTERMSIG(childStatus);
    } else {
        exitCode = childStatus;
    }
    r.exitCode = exitCode;

    // 6) 若启用输出捕获功能，则填充对应的输出字段
    if (cfg.captureOutput) {
        r.stdoutData = stdoutBuf;
        r.stderrData = stderrBuf;
    }

    // 7) 根据最终状态确定任务的整体执行结果
    if (timedOut) {
        r.status  = core::TaskStatus::Timeout;
        r.message = "TaskRunner：Shell执行超时";
    } else if (canceled) {
        r.status  = core::TaskStatus::Canceled;
        r.message = "TaskRunner：在Shell执行期间取消";
    } else if (exitCode == 0) {
        r.status = core::TaskStatus::Success;
    } else {
        r.status  = core::TaskStatus::Failed;
        r.message = "TaskRunner：Shell退出代码 " + std::to_string(exitCode);
    }

    // 记录本次执行所花费的时间
    const auto end = SteadyClock::now();
    r.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // ✅ 将 stdout/stderr 按行写入 LogManager buffer（用于 WS 实时订阅/分页拉取）
    if (cfg.captureOutput) {
        emit_lines_to_log_buffer(cfg.id, true,  stdoutBuf);
        emit_lines_to_log_buffer(cfg.id, false, stderrBuf);
    }

    // ✅ 结束事件（统一可观测性）
    {
        const int st = static_cast<int>(r.status);
        std::string endMsg = "Shell end: status=" + std::to_string(st) +
                             ", exitCode=" + std::to_string(exitCode) +
                             ", durationMs=" + std::to_string(r.durationMs);
        if (!r.message.empty()) {
            endMsg += ", message=" + r.message;
        }

        LogLevel lvl = LogLevel::Info;
        if (r.status == core::TaskStatus::Failed)   lvl = LogLevel::Error;
        if (r.status == core::TaskStatus::Timeout)  lvl = LogLevel::Warn;
        if (r.status == core::TaskStatus::Canceled) lvl = LogLevel::Warn;

        core::emitEvent(cfg,
                  lvl,
                  endMsg,
                  r.durationMs,
                  /*attempt*/1,
                  {
                      {"exit_code", std::to_string(exitCode)},
                      {"status", TaskStatusTypetoString(r.status)}
                  });
    }

    // 输出日志记录执行情况
    Logger::info("ShellExecutionStrategy::execute, id=" + cfg.id.value +
                ", name=" + cfg.name +
                ", status=" + std::to_string(static_cast<int>(r.status)) +
                ", message=" + r.message +
                ", duration=" + std::to_string(r.durationMs) + "ms");

    return r;
}

} // namespace taskhub::runner