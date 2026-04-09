#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <optional>
#include <filesystem>

namespace pw {

// Execute a shell command and return stdout. Returns empty string on failure.
std::string exec_command(const std::string& cmd, int timeout_ms = 5000);

// Format RSS in KB to human-readable string
std::string format_memory(uint64_t rss_kb);

// Format milliseconds to human-readable uptime (e.g. "2h 15m")
std::string format_uptime(int64_t ms);

// Truncate a string to max length, adding ellipsis if needed
std::string truncate(std::string_view str, size_t max);

// Walk up from dir looking for project root markers (package.json, Cargo.toml, etc.)
std::string find_project_root(const std::string& dir);

// Get basename from a path string
std::string path_basename(const std::string& path);

// Summarize a full command string to a short description
std::string summarize_command(const std::string& command, const std::string& process_name);

} // namespace pw
