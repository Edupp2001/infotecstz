#include "Logger/Logger.h"

#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <string>

using namespace LoggerLib;

struct LogTask {
    std::string message;
    LogLevel level;
};

std::queue<LogTask> log_queue;
std::mutex queue_mutex;
std::condition_variable queue_cv;
bool done_flag = false;

// part 2,1,c - logger worker thread: reads from queue and forwards to Logger
void logger_thread_func(std::shared_ptr<Logger> logger) {
    while (true) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        queue_cv.wait(lock, [] { return !log_queue.empty() || done_flag; });

        while (!log_queue.empty()) {
            LogTask task = log_queue.front();
            log_queue.pop();
            lock.unlock();

            logger->Log(task.message, task.level);

            lock.lock();
        }

        if (done_flag) break;
    }
}

// parse helper
LogLevel parse_level(const std::string& s) {
    if (s == "error") return LogLevel::Error;
    if (s == "warning") return LogLevel::Warning;
    return LogLevel::Info;
}

void print_usage(const char* prog) {
    std::cout << "Usage:\n"
              << prog << " <log_file> <default_level: error|warning|info> [socket_host socket_port]\n\n"
              << "Examples:\n"
              << prog << " log.txt info\n"
              << prog << " log.txt warning 127.0.0.1 5000\n";
}

int main(int argc, char* argv[]) {
    // part 2,2 - parameters: filename and default level; optional socket host+port
    if (argc != 3 && argc != 5) {
        print_usage(argv[0]);
        return 1;
    }

    std::string log_filename = argv[1];
    LogLevel default_level = parse_level(argv[2]);

    std::string socket_host;
    uint16_t socket_port = 0;
    if (argc == 5) {
        socket_host = argv[3];
        int p = std::stoi(argv[4]);
        if (p <= 0 || p > 65535) {
            std::cerr << "Invalid port\n";
            return 1;
        }
        socket_port = static_cast<uint16_t>(p);
    }

    // part 2,1,a & 1.6 - create logger with file and optional socket destination(s)
    auto logger = Logger::CreateWithFileAndOptionalSocket(log_filename, default_level, socket_host, socket_port);

    // start logger thread
    std::thread worker(logger_thread_func, logger);

    std::cout << "Enter messages. Optional prefix: error:message or warning:message or info:message\n";
    std::cout << "Type 'exit' to stop.\n";

    std::string line;
    while (true) {
        if (!std::getline(std::cin, line)) break;
        if (line == "exit") break;

        LogLevel lvl = default_level;
        std::string msg = line;

        // parse optional prefix
        auto pos = line.find(':');
        if (pos != std::string::npos) {
            std::string prefix = line.substr(0, pos);
            // lower-case compare
            for (auto &c : prefix) c = static_cast<char>(tolower(c));
            if (prefix == "error" || prefix == "warning" || prefix == "info") {
                lvl = parse_level(prefix);
                msg = line.substr(pos + 1);
            }
        }

        // enqueue task (part 2,1,c)
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            log_queue.push({msg, lvl});
        }
        queue_cv.notify_one();
    }

    // signal finish
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        done_flag = true;
    }
    queue_cv.notify_one();

    worker.join();
    return 0;
}
