#include "detection.hpp"
#include "display.hpp"
#include "process.hpp"
#include "scanner.hpp"
#include "util.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <fmt/color.h>
#include <fmt/format.h>

int main(int argc, char** argv) {
    CLI::App app{"Port Whisperer -- listen to your ports"};
    app.set_help_flag(""); // We handle help ourselves

    bool show_all = false;
    app.add_flag("--all,-a", show_all, "Show all ports/processes");

    bool show_help = false;
    app.add_flag("--help,-h", show_help, "Show help");

    // Subcommands
    auto* ps_cmd = app.add_subcommand("ps", "Show dev processes");
    ps_cmd->add_flag("--all,-a", show_all, "Show all processes");
    auto* kill_cmd = app.add_subcommand("kill", "Kill by port or PID");
    auto* clean_cmd = app.add_subcommand("clean", "Kill orphaned processes");
    auto* watch_cmd = app.add_subcommand("watch", "Monitor port changes");

    bool force = false;
    kill_cmd->add_flag("-f,--force", force, "Use SIGKILL instead of SIGTERM");
    std::vector<std::string> kill_targets;
    kill_cmd->add_option("targets", kill_targets, "Port numbers or PIDs to kill");

    app.allow_extras(true);
    app.require_subcommand(0, 1);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        if (e.get_exit_code() != 0) {
            fmt::print(stderr, "\n  {}\n  Run {} for usage.\n\n",
                       fmt::format(fmt::fg(fmt::color::red), "Error: {}", e.what()),
                       fmt::format(fmt::fg(fmt::color::cyan), "ports --help"));
        }
        return e.get_exit_code();
    }

    // Help
    if (show_help) {
        pw::display_help();
        return 0;
    }

    // ports ps
    if (ps_cmd->parsed()) {
        auto processes = pw::get_all_processes();
        if (!show_all) {
            // Filter to dev processes
            std::vector<pw::ProcessInfo> filtered;
            std::vector<pw::ProcessInfo> docker_procs;
            for (auto& p : processes) {
                if (!pw::is_dev_process(p.process_name, p.command)) continue;
                const auto& n = p.process_name;
                if (n.find("com.docke") == 0 || n.find("Docker") == 0 ||
                    n == "docker" || n == "docker-sandbox") {
                    docker_procs.push_back(std::move(p));
                } else {
                    filtered.push_back(std::move(p));
                }
            }
            // Collapse Docker into single summary
            if (!docker_procs.empty()) {
                double total_cpu = 0;
                double total_rss_kb = 0;
                for (const auto& dp : docker_procs) {
                    total_cpu += dp.cpu;
                    if (dp.memory) {
                        // Parse "X.Y MB" / "X.Y GB" / "X KB" back
                        double val = 0;
                        char unit[4] = {};
                        if (sscanf(dp.memory->c_str(), "%lf %2s", &val, unit) == 2) {
                            if (unit[0] == 'G') total_rss_kb += val * 1048576;
                            else if (unit[0] == 'M') total_rss_kb += val * 1024;
                            else total_rss_kb += val;
                        }
                    }
                }
                pw::ProcessInfo docker_summary{
                    .pid = docker_procs[0].pid,
                    .process_name = "Docker",
                    .command = "",
                    .description = fmt::format("{} processes", docker_procs.size()),
                    .cpu = total_cpu,
                    .memory = pw::format_memory(static_cast<uint64_t>(total_rss_kb)),
                };
                docker_summary.framework = "Docker";
                docker_summary.uptime = docker_procs[0].uptime;
                filtered.push_back(std::move(docker_summary));
            }
            processes = std::move(filtered);
        }
        std::sort(processes.begin(), processes.end(),
                  [](const pw::ProcessInfo& a, const pw::ProcessInfo& b) { return b.cpu < a.cpu; });
        pw::display_process_table(processes, !show_all);
        return 0;
    }

    // ports kill
    if (kill_cmd->parsed()) {
        if (kill_targets.empty()) {
            fmt::print(stderr, "\n  {}\n  {}\n\n",
                       fmt::format(fmt::fg(fmt::color::red), "Usage: ports kill [-f|--force] <port|pid> [port|pid...]"),
                       fmt::format(fmt::fg(fmt::color::gray), "Kills listener on port (1-65535), or process by PID. Use -f for SIGKILL."));
            return 1;
        }

        auto signal = force ? pw::KillSignal::Kill : pw::KillSignal::Term;
        std::string sig_name = force ? "SIGKILL" : "SIGTERM";
        bool any_failed = false;

        fmt::print("\n");
        for (const auto& arg : kill_targets) {
            int n;
            try {
                n = std::stoi(arg);
                if (std::to_string(n) != arg) throw std::exception();
            } catch (...) {
                fmt::print("  {} \"{}\" is not a valid port/PID\n",
                           fmt::format(fmt::fg(fmt::color::red), "\xe2\x9c\x95"), arg);
                any_failed = true;
                continue;
            }

            auto resolved = pw::resolve_kill_target(n);
            if (!resolved) {
                std::string msg = (n <= 65535)
                    ? fmt::format("No listener on :{} and no process with PID {}", n, n)
                    : fmt::format("No process with PID {}", n);
                fmt::print("  {} {}\n", fmt::format(fmt::fg(fmt::color::red), "\xe2\x9c\x95"), msg);
                any_failed = true;
                continue;
            }

            std::string label;
            if (resolved->via == pw::KillTarget::PORT) {
                auto name = resolved->info ? resolved->info->process_name : "unknown";
                label = fmt::format(":{} \xe2\x80\x94 {} (PID {})", resolved->port, name, resolved->pid);
            } else {
                label = fmt::format("PID {}", resolved->pid);
            }

            fmt::print("  Killing {}\n", label);
            if (pw::kill_process(resolved->pid, signal)) {
                fmt::print("  {} Sent {} to {}\n",
                           fmt::format(fmt::fg(fmt::color::green), "\xe2\x9c\x93"), sig_name, label);
            } else {
                fmt::print("  {} Failed. Try: sudo kill{} {}\n",
                           fmt::format(fmt::fg(fmt::color::red), "\xe2\x9c\x95"),
                           force ? " -9" : "", resolved->pid);
                any_failed = true;
            }
        }
        fmt::print("\n");
        return any_failed ? 1 : 0;
    }

    // ports clean
    if (clean_cmd->parsed()) {
        auto orphaned = pw::find_orphaned_processes();
        std::vector<pid_t> killed, failed;

        if (orphaned.empty()) {
            pw::display_clean_results(orphaned, killed, failed);
            return 0;
        }

        // Show what we found
        fmt::print("\n");
        fmt::print("  {}:\n",
                   fmt::format(fmt::fg(fmt::color::yellow) | fmt::emphasis::bold,
                               "Found {} orphaned/zombie process{}", orphaned.size(),
                               orphaned.size() == 1 ? "" : "es"));
        for (const auto& p : orphaned) {
            fmt::print("  {} :{} \xe2\x80\x94 {} {}\n",
                       fmt::format(fmt::fg(fmt::color::gray), "\xe2\x80\xa2"),
                       fmt::format(fmt::emphasis::bold, "{}", p.port),
                       p.process_name,
                       fmt::format(fmt::fg(fmt::color::gray), "(PID {})", p.pid));
        }
        fmt::print("\n");

        // Confirm
        fmt::print("  {} ", fmt::format(fmt::fg(fmt::color::yellow), "Kill all? [y/N]"));
        std::string answer;
        std::getline(std::cin, answer);

        if (!answer.empty() && (answer[0] == 'y' || answer[0] == 'Y')) {
            for (const auto& p : orphaned) {
                if (pw::kill_process(p.pid)) {
                    killed.push_back(p.pid);
                } else {
                    failed.push_back(p.pid);
                }
            }
            pw::display_clean_results(orphaned, killed, failed);
        } else {
            fmt::print("\n  {}\n\n", fmt::format(fmt::fg(fmt::color::gray), "Aborted."));
        }
        return 0;
    }

    // ports watch
    if (watch_cmd->parsed()) {
        pw::display_watch_header();
        pw::watch_ports([](std::string_view type, const pw::PortInfo& info) {
            pw::display_watch_event(type, info);
        }, 2000);
        fmt::print("\n\n  {}\n\n", fmt::format(fmt::fg(fmt::color::gray), "Stopped watching."));
        return 0;
    }

    // Default: check for port number in remaining args, or show port table
    auto extras = app.remaining();
    if (!extras.empty()) {
        try {
            int port = std::stoi(extras[0]);
            if (port > 0 && port <= 65535) {
                auto info = pw::get_port_details(static_cast<uint16_t>(port));
                pw::display_port_detail(info);
                return 0;
            }
        } catch (...) {}

        // Unknown command
        fmt::print("\n  {}\n  {}\n\n",
                   fmt::format(fmt::fg(fmt::color::red), "Unknown command: {}", extras[0]),
                   fmt::format(fmt::fg(fmt::color::gray), "Run {} for usage.",
                               fmt::format(fmt::fg(fmt::color::cyan), "ports --help")));
        return 1;
    }

    // Default: show port table
    auto ports = pw::get_listening_ports();
    if (!show_all) {
        std::vector<pw::PortInfo> filtered;
        for (auto& p : ports) {
            if (pw::is_dev_process(p.process_name, p.command)) {
                filtered.push_back(std::move(p));
            }
        }
        ports = std::move(filtered);
    }
    pw::display_port_table(ports, !show_all);
    return 0;
}
