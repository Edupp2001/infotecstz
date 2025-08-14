#include <iostream>
#include <string>
#include <sstream>
#include <deque>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <netinet/in.h>
#include <unistd.h>

// Структура для статистики
struct Stats {
    size_t total_messages = 0;
    size_t errors = 0;
    size_t warnings = 0;
    size_t infos = 0;

    size_t min_len = SIZE_MAX;
    size_t max_len = 0;
    double avg_len = 0.0;

    std::deque<std::chrono::system_clock::time_point> last_hour_msgs;
};

// Глобальные переменные
Stats stats;
std::mutex stats_mutex;
std::atomic<bool> changed(false);
std::atomic<bool> running(true);

void print_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex);

    std::cout << "\n===== Statistics =====\n";
    std::cout << "Total messages: " << stats.total_messages << "\n";
    std::cout << "Errors: " << stats.errors
              << ", Warnings: " << stats.warnings
              << ", Infos: " << stats.infos << "\n";

    // Чистим старые сообщения старше 1 часа
    auto now = std::chrono::system_clock::now();
    while (!stats.last_hour_msgs.empty() &&
           std::chrono::duration_cast<std::chrono::hours>(now - stats.last_hour_msgs.front()).count() >= 1) {
        stats.last_hour_msgs.pop_front();
    }
    std::cout << "Messages in last hour: " << stats.last_hour_msgs.size() << "\n";

    if (stats.total_messages > 0) {
        std::cout << "Min length: " << stats.min_len << "\n";
        std::cout << "Max length: " << stats.max_len << "\n";
        std::cout << "Avg length: " << stats.avg_len << "\n";
    } else {
        std::cout << "No messages yet.\n";
    }
    std::cout << "======================\n";
}

// Поток для таймера T
void timer_thread_func(int T) {
    auto last_print_time = std::chrono::steady_clock::now();
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_print_time).count() >= T) {
            if (changed.exchange(false)) {
                print_stats();
                last_print_time = now;
            }
        }
    }
}

// Обновление статистики
void update_stats(const std::string& message) {
    std::lock_guard<std::mutex> lock(stats_mutex);

    stats.total_messages++;
    size_t len = message.size();
    stats.min_len = std::min(stats.min_len, len);
    stats.max_len = std::max(stats.max_len, len);
    stats.avg_len = ((stats.avg_len * (stats.total_messages - 1)) + len) / stats.total_messages;

    // Определение уровня по содержимому
    if (message.find("[Error]") != std::string::npos) stats.errors++;
    else if (message.find("[Warning]") != std::string::npos) stats.warnings++;
    else if (message.find("[Info]") != std::string::npos) stats.infos++;

    stats.last_hour_msgs.push_back(std::chrono::system_clock::now());
    changed.store(true);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <port> <N> <T>\n";
        return 1;
    }

    int port = std::stoi(argv[1]);
    int N = std::stoi(argv[2]);
    int T = std::stoi(argv[3]);

    // Создание сокета
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    std::cout << "Listening on port " << port << "...\n";
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("accept");
        close(server_fd);
        return 1;
    }

    std::cout << "Client connected.\n";

    // Запуск таймера
    std::thread timer_thread(timer_thread_func, T);

    char buffer[1024];
    size_t last_stat_count = 0;
    while (true) {
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) break;
        buffer[bytes_read] = '\0';

        std::istringstream iss(buffer);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            std::cout << line << "\n"; // Вывод самого сообщения
            update_stats(line);

            if (stats.total_messages - last_stat_count >= (size_t)N) {
                print_stats();
                last_stat_count = stats.total_messages;
                changed.store(false);
            }
        }
    }

    running = false;
    timer_thread.join();
    close(client_fd);
    close(server_fd);

    std::cout << "Client disconnected.\n";
    return 0;
}
