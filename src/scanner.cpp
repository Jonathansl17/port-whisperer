#include "scanner.hpp"
#include "detection.hpp"
#include "docker.hpp"
#include "platform/platform.hpp"
#include "process.hpp"
#include "util.hpp"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <ctime>
#include <set>
#include <thread>

#include <fmt/format.h>

namespace pw {

namespace {

volatile sig_atomic_t g_watch_running = 0;

void watch_signal_handler(int) {
    g_watch_running = 0;
}

} // anonymous namespace

std::vector<PortInfo> get_listening_ports(bool detailed) {
    auto entries = platform::get_listening_ports();
    if (entries.empty()) return {};

    // Collect unique PIDs
    std::set<pid_t> pid_set;
    for (const auto& e : entries) pid_set.insert(e.pid);
    std::vector<pid_t> unique_pids(pid_set.begin(), pid_set.end());

    // Batch calls
    auto ps_map = batch_ps_info(unique_pids);
    auto cwd_map = platform::batch_get_cwd(unique_pids);

    // Docker detection (only if any docker process found)
    bool has_docker = std::any_of(entries.begin(), entries.end(), [](const RawPortEntry& e) {
        return e.process_name.find("com.docke") == 0 || e.process_name == "docker";
    });
    auto docker_map = has_docker ? batch_docker_info() : std::map<uint16_t, DockerContainer>{};

    std::vector<PortInfo> results;
    results.reserve(entries.size());

    for (const auto& entry : entries) {
        PortInfo info{
            .port = entry.port,
            .pid = entry.pid,
            .process_name = entry.process_name,
            .raw_name = entry.process_name,
            .status = "healthy"
        };

        // Enrich from ps
        auto ps_it = ps_map.find(entry.pid);
        if (ps_it != ps_map.end()) {
            const auto& ps = ps_it->second;
            info.command = ps.command;

            if (ps.stat.find('Z') != std::string::npos) {
                info.status = "zombie";
            } else if (ps.ppid == 1 && is_dev_process(entry.process_name, ps.command)) {
                info.status = "orphaned";
            }

            if (ps.rss_kb > 0) info.memory = format_memory(ps.rss_kb);

            // Parse lstart for uptime
            if (!ps.lstart.empty()) {
                struct tm tm_val = {};
                if (strptime(ps.lstart.c_str(), "%b %d %H:%M:%S %Y", &tm_val)) {
                    auto start = std::chrono::system_clock::from_time_t(mktime(&tm_val));
                    info.start_time = start;
                    auto now = std::chrono::system_clock::now();
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                    if (ms > 0) info.uptime = format_uptime(ms);
                }
            }

            // Framework from command line
            info.framework = detect_framework_from_command(ps.command, entry.process_name);
        }

        // Docker container detection
        auto docker_it = docker_map.find(entry.port);
        if (docker_it != docker_map.end()) {
            info.project_name = docker_it->second.name;
            info.framework = detect_framework_from_image(docker_it->second.image);
            info.process_name = "docker";
        }

        // Cwd + project + framework (skip if docker already set)
        auto cwd_it = cwd_map.find(entry.pid);
        if (cwd_it != cwd_map.end() && docker_it == docker_map.end()) {
            std::string project_root = find_project_root(cwd_it->second);
            info.cwd = project_root;
            info.project_name = path_basename(project_root);
            if (!info.framework) {
                info.framework = detect_framework(project_root);
            }

            if (detailed) {
                std::string branch = exec_command(
                    fmt::format("git -C \"{}\" rev-parse --abbrev-ref HEAD", project_root), 3000);
                if (!branch.empty()) info.git_branch = branch;
            }
        }

        // Process tree only in detailed mode
        if (detailed) {
            info.process_tree = get_process_tree(entry.pid);
        }

        results.push_back(std::move(info));
    }

    std::sort(results.begin(), results.end(),
              [](const PortInfo& a, const PortInfo& b) { return a.port < b.port; });

    return results;
}

std::optional<PortInfo> get_port_details(uint16_t target_port) {
    auto ports = get_listening_ports(true);
    for (auto& p : ports) {
        if (p.port == target_port) return std::move(p);
    }
    return std::nullopt;
}

std::vector<PortInfo> find_orphaned_processes() {
    auto ports = get_listening_ports();
    std::vector<PortInfo> orphaned;
    for (auto& p : ports) {
        if (p.status == "orphaned" || p.status == "zombie") {
            orphaned.push_back(std::move(p));
        }
    }
    return orphaned;
}

void watch_ports(WatchCallback callback, int interval_ms) {
    g_watch_running = 1;

    // Install signal handler for graceful exit
    struct sigaction sa{};
    sa.sa_handler = watch_signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);

    std::set<uint16_t> previous_ports;

    while (g_watch_running) {
        auto current = get_listening_ports();
        std::set<uint16_t> current_set;
        for (const auto& p : current) current_set.insert(p.port);

        // Check for new ports
        for (const auto& p : current) {
            if (previous_ports.count(p.port) == 0) {
                callback("new", p);
            }
        }

        // Check for removed ports
        for (uint16_t port : previous_ports) {
            if (current_set.count(port) == 0) {
                PortInfo removed{.port = port};
                callback("removed", removed);
            }
        }

        previous_ports = current_set;
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
}

} // namespace pw
