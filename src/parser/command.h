#pragma once
#include <string>
#include <vector>

enum class CommandType {
    SET, GET, DEL, TTL, EXISTS, PING, UNKNOWN
};

struct Command {
    CommandType type;
    std::vector<std::string> args;
    bool valid;
    std::string error_msg;
};

class CommandParser {
public:
    static Command parse(const std::string& raw_input);
private:
    static std::vector<std::string> tokenize(const std::string& input);
};
