#include "platform.hpp"
#include "../util.hpp"

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <fmt/format.h>

namespace pw::platform {

namespace {

struct LsofRecord {
    pid_t pid = 0;
    std::string process_name;
    std::vector<std::string> names;
};

std::vector<LsofRecord> parse_lsof_field_output(const std::string& raw) {
    std::vector<LsofRecord> records;
    LsofRecord* current = nullptr;

    std::istringstream iss(raw);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        char field = line[0];
        std::string value = line.substr(1);

        if (field == 'p') {
            try {
                pid_t pid = std::stoi(value);
                records.push_back({.pid = pid});
                current = &records.back();
            } catch (...) {
                current = nullptr;
            }
            continue;
        }

        if (!current) continue;

        if (field == 'c') current->process_name = value;
        else if (field == 'n') current->names.push_back(value);
    }

    return records;
}

uint16_t parse_port_from_address(const std::string& address) {
    auto pos = address.rfind(':');
    if (pos == std::string::npos) return 0;

    // Handle "addr:port->..." format
    std::string port_str = address.substr(pos + 1);
    auto arrow = port_str.find("->");
    if (arrow != std::string::npos) {
        port_str = port_str.substr(0, arrow);
    }

    try { return static_cast<uint16_t>(std::stoi(port_str)); }
    catch (...) { return 0; }
}

} // anonymous namespace

std::vector<RawPortEntry> get_listening_ports() {
    std::string raw = exec_command("lsof -nP -iTCP -sTCP:LISTEN -Fpcn", 10000);
    if (raw.empty()) return {};

    auto records = parse_lsof_field_output(raw);
    std::vector<RawPortEntry> entries;
    std::set<uint16_t> seen_ports;

    for (const auto& record : records) {
        for (const auto& address : record.names) {
            uint16_t port = parse_port_from_address(address);
            if (port == 0 || seen_ports.count(port)) continue;
            seen_ports.insert(port);
            entries.push_back({
                .port = port,
                .pid = record.pid,
                .process_name = record.process_name
            });
        }
    }

    return entries;
}

std::optional<std::string> get_process_cwd(pid_t pid) {
    std::string raw = exec_command(
        fmt::format("lsof -a -d cwd -p {} -Fpn", pid), 5000);
    if (raw.empty()) return std::nullopt;

    auto records = parse_lsof_field_output(raw);
    for (const auto& record : records) {
        for (const auto& name : record.names) {
            if (!name.empty() && name[0] == '/') return name;
        }
    }
    return std::nullopt;
}

std::map<pid_t, std::string> batch_get_cwd(const std::vector<pid_t>& pids) {
    std::map<pid_t, std::string> result;
    if (pids.empty()) return result;

    // Build comma-separated PID list for single lsof call
    std::string pid_list;
    for (size_t i = 0; i < pids.size(); ++i) {
        if (i > 0) pid_list += ',';
        pid_list += std::to_string(pids[i]);
    }

    std::string raw = exec_command(
        fmt::format("lsof -a -d cwd -p {} -Fpn", pid_list), 10000);

    if (!raw.empty()) {
        auto records = parse_lsof_field_output(raw);
        for (const auto& record : records) {
            for (const auto& name : record.names) {
                if (!name.empty() && name[0] == '/') {
                    result[record.pid] = name;
                    break;
                }
            }
        }
    }

    return result;
}

} // namespace pw::platform
