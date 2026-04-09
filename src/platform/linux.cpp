#include "platform.hpp"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unistd.h>

#include <fmt/format.h>

namespace fs = std::filesystem;

namespace pw::platform {

namespace {

// Parse a hex port from /proc/net/tcp format "addr:port"
uint16_t parse_hex_port(const std::string& addr_port) {
    auto pos = addr_port.find(':');
    if (pos == std::string::npos) return 0;
    return static_cast<uint16_t>(std::stoul(addr_port.substr(pos + 1), nullptr, 16));
}

// Parse inode from /proc/net/tcp line
uint64_t parse_inode(const std::string& line) {
    std::istringstream iss(line);
    std::string token;
    // Fields: sl, local_address, rem_address, st, tx_queue:rx_queue,
    //         tr:tm->when, retrnsmt, uid, timeout, inode
    for (int i = 0; i < 10 && iss >> token; ++i) {
        if (i == 9) return std::stoull(token);
    }
    return 0;
}

// Get process name from /proc/<pid>/comm
std::string get_comm(pid_t pid) {
    std::ifstream f(fmt::format("/proc/{}/comm", pid));
    std::string name;
    if (f && std::getline(f, name)) {
        // Trim trailing newline
        while (!name.empty() && (name.back() == '\n' || name.back() == '\r'))
            name.pop_back();
        return name;
    }
    return "";
}

// Build a map from socket inode -> pid by scanning /proc/<pid>/fd/
std::map<uint64_t, pid_t> build_inode_to_pid(const std::set<uint64_t>& target_inodes) {
    std::map<uint64_t, pid_t> result;
    if (target_inodes.empty()) return result;

    for (const auto& entry : fs::directory_iterator("/proc")) {
        if (!entry.is_directory()) continue;
        const auto& dirname = entry.path().filename().string();

        // Only look at numeric directories (PIDs)
        pid_t pid = 0;
        try { pid = std::stoi(dirname); } catch (...) { continue; }
        if (pid <= 0) continue;

        auto fd_path = entry.path() / "fd";
        DIR* dir = opendir(fd_path.c_str());
        if (!dir) continue;

        char link_buf[256];
        struct dirent* de;
        while ((de = readdir(dir)) != nullptr) {
            if (de->d_name[0] == '.') continue;

            auto link_path = fd_path / de->d_name;
            ssize_t len = readlink(link_path.c_str(), link_buf, sizeof(link_buf) - 1);
            if (len <= 0) continue;
            link_buf[len] = '\0';

            // Check for "socket:[inode]"
            if (strncmp(link_buf, "socket:[", 8) != 0) continue;
            uint64_t inode = std::stoull(link_buf + 8);

            if (target_inodes.count(inode)) {
                result[inode] = pid;
                // Early exit if we found all
                if (result.size() == target_inodes.size()) {
                    closedir(dir);
                    return result;
                }
            }
        }
        closedir(dir);
    }
    return result;
}

// Parse /proc/net/tcp or /proc/net/tcp6 for LISTEN sockets
// Returns map of inode -> port for sockets in LISTEN state (st == 0A)
std::map<uint64_t, uint16_t> parse_proc_net_tcp(const std::string& path) {
    std::map<uint64_t, uint16_t> result;
    std::ifstream f(path);
    if (!f) return result;

    std::string line;
    std::getline(f, line); // Skip header

    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string sl, local_addr, rem_addr, state;
        if (!(iss >> sl >> local_addr >> rem_addr >> state)) continue;

        // 0A = TCP_LISTEN
        if (state != "0A") continue;

        uint16_t port = parse_hex_port(local_addr);
        if (port == 0) continue;

        uint64_t inode = parse_inode(line);
        if (inode == 0) continue;

        // Keep first occurrence per port
        bool already_seen = false;
        for (const auto& [existing_inode, existing_port] : result) {
            if (existing_port == port) { already_seen = true; break; }
        }
        if (!already_seen) {
            result[inode] = port;
        }
    }
    return result;
}

} // anonymous namespace

std::vector<RawPortEntry> get_listening_ports() {
    // Parse both IPv4 and IPv6
    auto tcp4 = parse_proc_net_tcp("/proc/net/tcp");
    auto tcp6 = parse_proc_net_tcp("/proc/net/tcp6");

    // Merge, dedup by port (prefer tcp4 if both have same port)
    std::set<uint16_t> seen_ports;
    std::set<uint64_t> all_inodes;

    for (const auto& [inode, port] : tcp4) {
        seen_ports.insert(port);
        all_inodes.insert(inode);
    }
    for (const auto& [inode, port] : tcp6) {
        if (seen_ports.count(port) == 0) {
            all_inodes.insert(inode);
        }
    }

    // Resolve inodes to PIDs
    auto inode_to_pid = build_inode_to_pid(all_inodes);

    // Build result
    std::vector<RawPortEntry> entries;
    std::set<uint16_t> added_ports;

    auto process_map = [&](const std::map<uint64_t, uint16_t>& tcp_map) {
        for (const auto& [inode, port] : tcp_map) {
            if (added_ports.count(port)) continue;
            auto it = inode_to_pid.find(inode);
            if (it == inode_to_pid.end()) continue;

            added_ports.insert(port);
            entries.push_back({
                .port = port,
                .pid = it->second,
                .process_name = get_comm(it->second)
            });
        }
    };

    process_map(tcp4);
    process_map(tcp6);

    return entries;
}

std::optional<std::string> get_process_cwd(pid_t pid) {
    char buf[PATH_MAX];
    auto link = fmt::format("/proc/{}/cwd", pid);
    ssize_t len = readlink(link.c_str(), buf, sizeof(buf) - 1);
    if (len <= 0) return std::nullopt;
    buf[len] = '\0';
    return std::string(buf);
}

std::map<pid_t, std::string> batch_get_cwd(const std::vector<pid_t>& pids) {
    std::map<pid_t, std::string> result;
    for (pid_t pid : pids) {
        auto cwd = get_process_cwd(pid);
        if (cwd) {
            result[pid] = *cwd;
        }
    }
    return result;
}

} // namespace pw::platform
