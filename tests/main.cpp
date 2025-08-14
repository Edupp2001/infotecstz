#include "Logger/Logger.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <vector>

#define ASSERT_TRUE(expr) \
    do { if (!(expr)) { \
        std::cerr << "FAILED: " << __FUNCTION__ << " at line " << __LINE__ << ": " << #expr << std::endl; \
        return false; \
    } } while (0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))
#define ASSERT_CONTAINS(str, substr) ASSERT_TRUE((str).find(substr) != std::string::npos)



bool test_level_filtering() {
    auto logger = std::make_shared<LoggerLib::Logger>(LoggerLib::LogLevel::Warning);

    // Запишем в файл
    std::string filename = "test_log.txt";
    logger->AddFileDestination(filename);

    logger->Log("Error msg", LoggerLib::LogLevel::Error);   // должно записаться
    logger->Log("Warning msg", LoggerLib::LogLevel::Warning); // должно записаться
    logger->Log("Info msg", LoggerLib::LogLevel::Info);     // не должно записаться

    std::ifstream ifs(filename);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();
    std::remove(filename.c_str());

    ASSERT_CONTAINS(content, "Error msg");
    ASSERT_CONTAINS(content, "Warning msg");
    ASSERT_FALSE(content.find("Info msg") != std::string::npos);
    return true;
}

bool test_file_destination_write() {
    std::string filename = "test_file_dest.txt";
    {
        LoggerLib::FileDestination fileDest(filename);
        fileDest.WriteLogLine("Hello File");
    }
    std::ifstream ifs(filename);
    std::string content;
    std::getline(ifs, content);
    ifs.close();
    std::remove(filename.c_str());

    ASSERT_EQ(content, "Hello File");
    return true;
}

bool test_socket_destination_create() {
    // Пытаемся создать без работающего сервера
    LoggerLib::SocketDestination sockDest("127.0.0.1", 654321); // не должен падать
    // Просто вызываем метод записи — он должен корректно завершиться без исключений
    sockDest.WriteLogLine("Test over socket");
    return true;
}

int main() {
    std::vector<std::pair<std::string, bool(*)()>> tests = {
        {"LogLevel filtering works", test_level_filtering},
        {"FileDestination writes to file", test_file_destination_write},
        {"SocketDestination creates without server", test_socket_destination_create}
    };

    int passed = 0;
    for (auto& [name, func] : tests) {
        std::cout << "Running: " << name << " ... ";
        if (func()) {
            std::cout << "PASSED\n";
            passed++;
        } else {
            std::cout << "FAILED\n";
        }
    }

    std::cout << "\nSummary: " << passed << "/" << tests.size() << " tests passed.\n";
    return (passed == tests.size()) ? 0 : 1;
}
