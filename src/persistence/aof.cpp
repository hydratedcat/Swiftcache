#include "aof.h"

#include <iostream>

// ---------------------------------------------------------------------------
// AOFWriter
// ---------------------------------------------------------------------------
AOFWriter::AOFWriter(const std::string &filepath) : filepath_(filepath) {
  // Open in append mode — existing log is preserved across restarts.
  file_.open(filepath_, std::ios::app);
  if (!file_.is_open()) {
    std::cerr << "Warning: could not open AOF file '" << filepath_
              << "' for writing\n";
  }
}

AOFWriter::~AOFWriter() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (file_.is_open()) {
    file_.flush();
    file_.close();
  }
}

void AOFWriter::log_command(const std::string &raw_command) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!file_.is_open()) {
    return;
  }
  file_ << raw_command << '\n';
  file_.flush(); // ensure durability — every command hits OS buffer
}

void AOFWriter::fsync_now() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (file_.is_open()) {
    file_.flush();
  }
}

const std::string &AOFWriter::filepath() const { return filepath_; }

// ---------------------------------------------------------------------------
// AOFReader
// ---------------------------------------------------------------------------
AOFReader::AOFReader(const std::string &filepath) : filepath_(filepath) {}

std::vector<std::string> AOFReader::read_all_commands() {
  std::vector<std::string> commands;

  std::ifstream file(filepath_);
  if (!file.is_open()) {
    // No AOF file → cold start with empty cache (not an error)
    return commands;
  }

  std::string line;
  while (std::getline(file, line)) {
    // Strip trailing \r if present (cross-platform safety)
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (!line.empty()) {
      commands.push_back(line);
    }
  }

  return commands;
}
