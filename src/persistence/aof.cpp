#include "aof.h"

AOFWriter::AOFWriter(const std::string& filepath) : filepath_(filepath) {}
void AOFWriter::log_command(const std::string& /*raw_command*/) {}
void AOFWriter::fsync_now() {}
AOFWriter::~AOFWriter() = default;

AOFReader::AOFReader(const std::string& /*filepath*/) {}
std::vector<std::string> AOFReader::read_all_commands() { return {}; }
