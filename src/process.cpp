#include "process.hpp"
#include "detection.hpp"
#include "platform/platform.hpp"
#include "scanner.hpp"
#include "util.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <map>
#include <regex>
#include <set>
#include <signal.h>
#include <sstream>
#include <string>
#include <unistd.h>

#include <fmt/format.h>

namespace pw {

std::map<pid_t, PsEntry> batch_ps_info(const std::vector<pid_t>& pids) {
    std::map<pid_t, PsEntry> result;
    if (pids.empty()) return result;

    std::string pid_list;
    for (size_t i = 0; i < pids.size(); ++i) {
        if (i > 0) pid_list += ',';
        pid_list += std::to_string(pids[i]);
    }

    std::string raw = exec_command(
        fmt::format("ps -p {} -o pid=,ppid=,stat=,rss=,lstart=,command=", pid_list));
    if (raw.empty()) return result;

    // Format: PID PPID STAT RSS DOW MON DD HH:MM:SS YYYY COMMAND...
    static const std::regex line_re(
        R"(^\s*(\d+)\s+(\d+)\s+(\S+)\s+(\d+)\s+\w+\s+(\w+)\s+(\d+)\s+([\d:]+)\s+(\d+)\s+(.*)$)");

    std::istringstream iss(raw);
    std::string line;
    while (std::getline(iss, line)) {
        std::smatch m;
        if (!std::regex_match(line, m, line_re)) continue;

        pid_t pid = std::stoi(m[1]);
        result[pid] = PsEntry{
            .pid = pid,
            .ppid = std::stoi(m[2]),
            .stat = m[3],
            .rss_kb = std::stoull(m[4]),
            .lstart = fmt::format("{} {} {} {}", m[5].str(), m[6].str(), m[7].str(), m[8].str()),
            .command = m[9]
        };
    }

    return result;
}

std::vector<ProcessTreeNode> get_process_tree(pid_t target_pid) {
    std::vector<ProcessTreeNode> tree;
    std::string raw = exec_command("ps -eo pid=,ppid=,comm=");
    if (raw.empty()) return tree;

    std::map<pid_t, ProcessTreeNode> processes;
    std::istringstream iss(raw);
    std::string line;
    while (std::getline(iss, line)) {
        std::istringstream lss(line);
        int pid_val, ppid_val;
        std::string name;
        if (!(lss >> pid_val >> ppid_val)) continue;
        std::getline(lss >> std::ws, name);
        processes[pid_val] = {pid_val, ppid_val, name};
    }

    pid_t current = target_pid;
    int depth = 0;
    while (current > 1 && depth < 8) {
        auto it = processes.find(current);
        if (it == processes.end()) break;
        tree.push_back(it->second);
        current = it->second.ppid;
        ++depth;
    }
    return tree;
}

std::vector<ProcessInfo> get_all_processes() {
    std::string raw = exec_command("ps -eo pid=,pcpu=,pmem=,rss=,lstart=,command=");
    if (raw.empty()) return {};

    static const std::regex line_re(
        R"(^\s*(\d+)\s+([\d.]+)\s+([\d.]+)\s+(\d+)\s+\w+\s+(\w+)\s+(\d+)\s+([\d:]+)\s+(\d+)\s+(.*)$)");

    struct RawEntry {
        pid_t pid;
        std::string process_name;
        double cpu;
        double mem_percent;
        uint64_t rss;
        std::string lstart;
        std::string command;
    };

    std::vector<RawEntry> entries;
    std::set<pid_t> seen;
    pid_t my_pid = getpid();

    std::istringstream iss(raw);
    std::string line;
    while (std::getline(iss, line)) {
        std::smatch m;
        if (!std::regex_match(line, m, line_re)) continue;

        pid_t pid = std::stoi(m[1]);
        if (pid <= 1 || pid == my_pid || seen.count(pid)) continue;
        seen.insert(pid);

        std::string command = m[9];
        std::string pname = path_basename(command.substr(0, command.find(' ')));

        entries.push_back({
            .pid = pid,
            .process_name = pname,
            .cpu = std::stod(m[2]),
            .mem_percent = std::stod(m[3]),
            .rss = std::stoull(m[4]),
            .lstart = fmt::format("{} {} {} {}", m[5].str(), m[6].str(), m[7].str(), m[8].str()),
            .command = command
        });
    }

    // Batch cwd lookup for non-Docker processes
    std::vector<pid_t> cwd_pids;
    for (const auto& e : entries) {
        const auto& n = e.process_name;
        if (n.find("com.docke") == 0 || n.find("Docker") == 0 ||
            n == "docker" || n == "docker-sandbox") continue;
        cwd_pids.push_back(e.pid);
    }
    auto cwd_map = platform::batch_get_cwd(cwd_pids);

    std::vector<ProcessInfo> result;
    result.reserve(entries.size());

    for (const auto& e : entries) {
        ProcessInfo info{
            .pid = e.pid,
            .process_name = e.process_name,
            .command = e.command,
            .description = summarize_command(e.command, e.process_name),
            .cpu = e.cpu,
        };

        if (e.rss > 0) info.memory = format_memory(e.rss);

        // Parse lstart to compute uptime
        if (!e.lstart.empty()) {
            // lstart format: "Mon DD HH:MM:SS YYYY" — parsed by the system
            // We use a simplified approach via the raw string
            struct tm tm_val = {};
            if (strptime(e.lstart.c_str(), "%b %d %H:%M:%S %Y", &tm_val)) {
                auto start = std::chrono::system_clock::from_time_t(mktime(&tm_val));
                auto now = std::chrono::system_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                if (ms > 0) info.uptime = format_uptime(ms);
            }
        }

        info.framework = detect_framework_from_command(e.command, e.process_name);

        auto cwd_it = cwd_map.find(e.pid);
        if (cwd_it != cwd_map.end()) {
            std::string project_root = find_project_root(cwd_it->second);
            info.cwd = project_root;
            info.project_name = path_basename(project_root);
            if (!info.framework) {
                info.framework = detect_framework(project_root);
            }
        }

        result.push_back(std::move(info));
    }

    return result;
}

bool pid_exists(pid_t pid) {
    return kill(pid, 0) == 0;
}

bool kill_process(pid_t pid, KillSignal sig) {
    int signal = (sig == KillSignal::Kill) ? SIGKILL : SIGTERM;
    return kill(pid, signal) == 0;
}

std::optional<KillTarget> resolve_kill_target(int n) {
    if (n < 1) return std::nullopt;

    if (n <= 65535) {
        auto details = get_port_details(static_cast<uint16_t>(n));
        if (details) {
            return KillTarget{
                .pid = details->pid,
                .via = KillTarget::PORT,
                .port = static_cast<uint16_t>(n),
                .info = *details
            };
        }
    }

    if (pid_exists(static_cast<pid_t>(n))) {
        return KillTarget{
            .pid = static_cast<pid_t>(n),
            .via = KillTarget::PID,
        };
    }

    return std::nullopt;
}

} // namespace pw
