#pragma once
#include <fstream>
#include <string>
#include <mutex>
#include <vector>

class AOFWriter {
public:
    explicit AOFWriter(const std::string& filepath);
    void log_command(const std::string& raw_command);
    void fsync_now();
    ~AOFWriter();

private:
    std::ofstream file_;
    std::mutex mutex_;
    std::string filepath_;
};

class AOFReader {
public:
    explicit AOFReader(const std::string& filepath);
    std::vector<std::string> read_all_commands();
};
