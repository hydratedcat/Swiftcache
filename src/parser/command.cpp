#include "command.h"

#include <algorithm>
#include <sstream>

// ---------------------------------------------------------------------------
// tokenize: split input on whitespace, collapse multiple spaces
// ---------------------------------------------------------------------------
std::vector<std::string> CommandParser::tokenize(const std::string &input) {
  std::vector<std::string> tokens;
  std::istringstream stream(input);
  std::string token;
  while (stream >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

// ---------------------------------------------------------------------------
// parse: classify command type & validate argument count
// ---------------------------------------------------------------------------
Command CommandParser::parse(const std::string &raw_input) {
  Command cmd;
  cmd.valid = false;

  std::vector<std::string> tokens = tokenize(raw_input);

  if (tokens.empty()) {
    cmd.type = CommandType::UNKNOWN;
    cmd.error_msg = "ERR empty command";
    return cmd;
  }

  // Normalise command name to uppercase for case-insensitive matching
  std::string name = tokens[0];
  std::transform(name.begin(), name.end(), name.begin(), ::toupper);

  // args = everything after the command name
  cmd.args = std::vector<std::string>(tokens.begin() + 1, tokens.end());

  if (name == "SET") {
    cmd.type = CommandType::SET;
    if (cmd.args.size() < 2) {
      cmd.error_msg = "ERR SET requires key and value";
    } else {
      cmd.valid = true;
    }
  } else if (name == "GET") {
    cmd.type = CommandType::GET;
    if (cmd.args.size() < 1) {
      cmd.error_msg = "ERR GET requires key";
    } else {
      cmd.valid = true;
    }
  } else if (name == "DEL") {
    cmd.type = CommandType::DEL;
    if (cmd.args.size() < 1) {
      cmd.error_msg = "ERR DEL requires key";
    } else {
      cmd.valid = true;
    }
  } else if (name == "TTL") {
    cmd.type = CommandType::TTL;
    if (cmd.args.size() < 2) {
      cmd.error_msg = "ERR TTL requires key and seconds";
    } else {
      // Validate that seconds is a non-negative integer
      try {
        int secs = std::stoi(cmd.args[1]);
        if (secs < 0) {
          cmd.error_msg = "ERR TTL seconds must be non-negative";
        } else {
          cmd.valid = true;
        }
      } catch (...) {
        cmd.error_msg = "ERR TTL seconds must be an integer";
      }
    }
  } else if (name == "EXISTS") {
    cmd.type = CommandType::EXISTS;
    if (cmd.args.size() < 1) {
      cmd.error_msg = "ERR EXISTS requires key";
    } else {
      cmd.valid = true;
    }
  } else if (name == "PING") {
    cmd.type = CommandType::PING;
    cmd.valid = true;
    // PING accepts no arguments — ignore any trailing tokens
  } else {
    cmd.type = CommandType::UNKNOWN;
    cmd.error_msg = "ERR unknown command '" + tokens[0] + "'";
  }

  return cmd;
}
