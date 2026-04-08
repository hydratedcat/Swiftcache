#include "command.h"

Command CommandParser::parse(const std::string& /*raw_input*/) {
    return Command{CommandType::UNKNOWN, {}, false, "not implemented"};
}

std::vector<std::string> CommandParser::tokenize(const std::string& /*input*/) {
    return {};
}
