#include "Logger/Logger.h"
#include <iomanip>
#include <sstream>
#include <iostream>
#include <cstring>
#include <cerrno>

#ifdef __linux__
#include <netdb.h>
#endif

namespace LoggerLib {

/* ---------------- FileDestination ---------------- */

// part 1,2,a - open file
FileDestination::FileDestination(const std::string& filename) {
    ofs_.open(filename, std::ios::app);
    // If cannot open, std::ofstream will be in fail state — we'll still keep object but writes will be no-ops
    if (!ofs_.is_open()) {
        // do not throw (requirement: handle errors gracefully)
        std::cerr << "FileDestination: failed to open file: " << filename << "\n";
    }
}

FileDestination::~FileDestination() {
    std::lock_guard<std::mutex> lock(file_mutex_);
    if (ofs_.is_open()) ofs_.close();
}

// part 1,3,a,b,c - thread-safe append
void FileDestination::WriteLogLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(file_mutex_);
    if (!ofs_.is_open()) return;
    ofs_ << line << std::endl;
    // flush to ensure delivery
    ofs_.flush();
}

/* ---------------- SocketDestination ---------------- */

SocketDestination::SocketDestination(const std::string& host, uint16_t port)
    : sockfd_(-1), connected_(false) {
#ifdef __linux__
    // Resolve host
    addrinfo hints{};
    addrinfo* res = nullptr;
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    int rc = getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (rc != 0 || res == nullptr) {
        std::cerr << "SocketDestination: getaddrinfo failed for " << host << ": " << gai_strerror(rc) << "\n";
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr = ((sockaddr_in*)res->ai_addr)->sin_addr;
    freeaddrinfo(res);

    sockfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0) {
        std::cerr << "SocketDestination: socket() failed: " << strerror(errno) << "\n";
        return;
    }

    // Try to connect (blocking). If fails, close socket and mark disconnected.
    if (::connect(sockfd_, (sockaddr*)&addr, sizeof(addr)) != 0) {
        std::cerr << "SocketDestination: connect() failed: " << strerror(errno) << "\n";
        ::close(sockfd_);
        sockfd_ = -1;
        connected_.store(false);
        return;
    }

    connected_.store(true);
#else
    // On non-linux, do nothing for now
    (void)host; (void)port;
#endif
}

SocketDestination::~SocketDestination() {
#ifdef __linux__
    std::lock_guard<std::mutex> lock(sock_mutex_);
    if (sockfd_ >= 0) {
        ::close(sockfd_);
        sockfd_ = -1;
    }
    connected_.store(false);
#endif
}

bool SocketDestination::IsConnected() const {
    return connected_.load();
}

// part 1.5 - send message over socket (one line + '\n')
void SocketDestination::WriteLogLine(const std::string& line) {
#ifdef __linux__
    std::lock_guard<std::mutex> lock(sock_mutex_);
    if (!connected_.load() || sockfd_ < 0) return;

    std::string out = line;
    if (out.empty() || out.back() != '\n') out.push_back('\n');

    ssize_t total_sent = 0;
    const char* data = out.c_str();
    ssize_t to_send = static_cast<ssize_t>(out.size());
    while (total_sent < to_send) {
        ssize_t sent = ::send(sockfd_, data + total_sent, to_send - total_sent, 0);
        if (sent < 0) {
            // on error, mark disconnected and stop
            std::cerr << "SocketDestination: send() failed: " << strerror(errno) << "\n";
            ::close(sockfd_);
            sockfd_ = -1;
            connected_.store(false);
            return;
        }
        total_sent += sent;
    }
#else
    (void)line;
#endif
}

/* ---------------- Logger ---------------- */

// part 1,2,b - create with default level
Logger::Logger(LogLevel default_level)
    : current_level_(default_level) {
}

// part 1,4 - set default level (thread-safe)
void Logger::SetLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(level_mutex_);
    current_level_ = level;
}

// get current level
LogLevel Logger::GetLogLevel() const {
    std::lock_guard<std::mutex> lock(level_mutex_);
    return current_level_;
}

// part 1.6 - add file destination
void Logger::AddFileDestination(const std::string& filename) {
    std::lock_guard<std::mutex> lock(destinations_mutex_);
    destinations_.push_back(std::make_unique<FileDestination>(filename));
}

// part 1.5 & 1.6 - add socket destination
void Logger::AddSocketDestination(const std::string& host, uint16_t port) {
    std::lock_guard<std::mutex> lock(destinations_mutex_);
    std::cout << "k1" << std::endl;
    destinations_.push_back(std::make_unique<SocketDestination>(host, port));
}

// part 1,3,a,b,c & 1.6 - log with explicit level (filtering applied here)
void Logger::Log(const std::string& message, LogLevel level) {
    // level check
    {
        std::lock_guard<std::mutex> lock(level_mutex_);
        if (static_cast<int>(level) > static_cast<int>(current_level_)) {
            return; // lower priority -> ignore
        }
    }

    std::string line = FormatLogLine(message, level);

    // iterate destinations and write (each destination is responsible for its own locking)
    std::lock_guard<std::mutex> lock(destinations_mutex_);
    for (auto& dest : destinations_) {
        if (dest) {
            try {
                dest->WriteLogLine(line);
            } catch (...) {
                // do not throw exceptions from logging - swallow errors
            }
        }
    }
}

// part 1,3,a,b,c - default level overload
void Logger::Log(const std::string& message) {
    Log(message, GetLogLevel());
}

// part 1,3,c) - timestamp generation
std::string Logger::GetTimestamp() const {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_time{};
#ifdef _WIN32
    localtime_s(&tm_time, &t);
#else
    localtime_r(&t, &tm_time);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_time, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string Logger::LevelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::Error: return "Error";
        case LogLevel::Warning: return "Warning";
        case LogLevel::Info: return "Info";
        default: return "Unknown";
    }
}

std::string Logger::FormatLogLine(const std::string& message, LogLevel level) const {
    std::ostringstream oss;
    oss << GetTimestamp() << " [" << LevelToString(level) << "] " << message;
    return oss.str();
}

// convenience factory
std::shared_ptr<Logger> Logger::CreateWithFileAndOptionalSocket(const std::string& filename,
                                                                LogLevel level,
                                                                const std::string& socket_host,
                                                                uint16_t socket_port) {
    auto logger = std::make_shared<Logger>(level);
    if (!filename.empty()) logger->AddFileDestination(filename);
    if (!socket_host.empty() && socket_port != 0) logger->AddSocketDestination(socket_host, socket_port);
    return logger;
}

} // namespace LoggerLib
