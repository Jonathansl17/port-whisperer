#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pw {

struct ProcessTreeNode {
    pid_t pid;
    pid_t ppid;
    std::string name;
};

struct PortInfo {
    uint16_t port;
    pid_t pid;
    std::string process_name;
    std::string raw_name;
    std::string command;
    std::optional<std::string> cwd;
    std::optional<std::string> project_name;
    std::optional<std::string> framework;
    std::optional<std::string> uptime;
    std::optional<std::chrono::system_clock::time_point> start_time;
    std::string status = "healthy";
    std::optional<std::string> memory;
    std::optional<std::string> git_branch;
    std::vector<ProcessTreeNode> process_tree;
};

struct ProcessInfo {
    pid_t pid;
    std::string process_name;
    std::string command;
    std::string description;
    double cpu;
    std::optional<std::string> memory;
    std::optional<std::string> cwd;
    std::optional<std::string> project_name;
    std::optional<std::string> framework;
    std::optional<std::string> uptime;
};

struct PsEntry {
    pid_t pid;
    pid_t ppid;
    std::string stat;
    uint64_t rss_kb;
    std::string lstart;
    std::string command;
};

struct DockerContainer {
    std::string name;
    std::string image;
};

enum class KillSignal { Term, Kill };

struct KillTarget {
    pid_t pid;
    enum Via { PORT, PID } via;
    uint16_t port = 0;
    std::optional<PortInfo> info;
};

struct RawPortEntry {
    uint16_t port;
    pid_t pid;
    std::string process_name;
};

} // namespace pw
