#pragma once
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

// Appends every write command (SET, DEL, TTL) to a log file on disk.
// Thread-safe — multiple Connection threads call log_command() concurrently.
class AOFWriter {
public:
  explicit AOFWriter(const std::string &filepath);
  ~AOFWriter();

  /// Appends the raw command string as one line; flushes immediately.
  void log_command(const std::string &raw_command);

  /// Forces an explicit fsync to guarantee data reaches disk.
  void fsync_now();

  /// Returns the path this writer logs to.
  [[nodiscard]] const std::string &filepath() const;

  // Non-copyable, non-movable (owns file handle)
  AOFWriter(const AOFWriter &) = delete;
  AOFWriter &operator=(const AOFWriter &) = delete;
  AOFWriter(AOFWriter &&) = delete;
  AOFWriter &operator=(AOFWriter &&) = delete;

private:
  std::string filepath_;
  std::ofstream file_;
  std::mutex mutex_;
};

// Reads an AOF log and returns every command line for replay.
class AOFReader {
public:
  explicit AOFReader(const std::string &filepath);

  /// Reads the entire file and returns each non-empty line.
  [[nodiscard]] std::vector<std::string> read_all_commands();

private:
  std::string filepath_;
};
