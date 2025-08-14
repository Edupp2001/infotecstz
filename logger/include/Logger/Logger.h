#pragma once

// part 1,2,b) enum levels
#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <memory>
#include <vector>
#include <atomic>

#ifdef __linux__
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace LoggerLib {

// part 1,2,b) - three log levels
enum class LogLevel {
    Error = 0,
    Warning = 1,
    Info = 2
};

// Interface for a log destination (file, socket, ...)
class ILogDestination {
public:
    virtual ~ILogDestination() = default;
    // part 1,3,a,b,c) - write one formatted log line to the destination
    virtual void WriteLogLine(const std::string& line) = 0;
};

// File destination implementation
class FileDestination : public ILogDestination {
public:
    // part 1,2,a) - constructed with filename
    explicit FileDestination(const std::string& filename);
    ~FileDestination() override;

    // part 1,3,a,b,c) - write a line to file (thread-safe)
    void WriteLogLine(const std::string& line) override;

private:
    std::ofstream ofs_;
    std::mutex file_mutex_;
};

// Socket destination implementation (TCP)
// part 1.5 - send logs to a TCP server (one-line per message)
class SocketDestination : public ILogDestination {
public:
    // part 1.5 - host (IP or hostname) and port; if connection fails, destination becomes inactive
    SocketDestination(const std::string& host, uint16_t port);
    ~SocketDestination() override;

    // part 1.5 - send a line over socket (thread-safe). If socket is down, do nothing.
    void WriteLogLine(const std::string& line) override;

    bool IsConnected() const;

private:
    int sockfd_;
    std::mutex sock_mutex_;
    std::atomic<bool> connected_;
#ifdef _WIN32
    // Windows-specific members could go here (not used in Linux build)
#endif
};

// Main Logger: aggregates multiple destinations and does filtering by level
class Logger {
public:
    // Non-copyable
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // part 1,2,a & 1,2,b - create with default log level (no destinations initially)
    explicit Logger(LogLevel default_level);

    // part 1,4 - set default log level at runtime
    void SetLogLevel(LogLevel level);

    // part 1,2,b - get current level
    LogLevel GetLogLevel() const;

    // part 1,2,a & 1.6 - add file destination (can be called multiple times)
    void AddFileDestination(const std::string& filename);

    // part 1.5 & 1.6 - add socket destination (non-blocking for the caller; internal connection attempt done in ctor)
    void AddSocketDestination(const std::string& host, uint16_t port);

    // part 1,3,a,b,c & 1.6 - log message with explicit level
    void Log(const std::string& message, LogLevel level);

    // part 1,3,a,b,c - log message using default level
    void Log(const std::string& message);

    // helper - convenience factory to create Logger with a file and optional socket
    static std::shared_ptr<Logger> CreateWithFileAndOptionalSocket(const std::string& filename,
                                                                   LogLevel level,
                                                                   const std::string& socket_host = "",
                                                                   uint16_t socket_port = 0);

private:
    // part 1,3,c) - generate timestamp string
    std::string GetTimestamp() const;

    // helper - convert LogLevel to string
    std::string LevelToString(LogLevel level) const;

    // format a log line with timestamp and level
    std::string FormatLogLine(const std::string& message, LogLevel level) const;

private:
    LogLevel current_level_;
    mutable std::mutex level_mutex_;

    std::vector<std::unique_ptr<ILogDestination>> destinations_;
    mutable std::mutex destinations_mutex_;
};

} // namespace LoggerLib
