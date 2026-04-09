#include "display.hpp"
#include "util.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>

#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <tabulate/table.hpp>

namespace pw {

namespace {

// ANSI escape helpers
namespace ansi {
    const std::string reset   = "\033[0m";
    const std::string bold    = "\033[1m";
    const std::string gray    = "\033[90m";
    const std::string red     = "\033[31m";
    const std::string green   = "\033[32m";
    const std::string yellow  = "\033[33m";
    const std::string blue    = "\033[34m";
    const std::string magenta = "\033[35m";
    const std::string cyan    = "\033[36m";
    const std::string white   = "\033[37m";
    const std::string bold_white  = "\033[1;37m";
    const std::string bold_cyan   = "\033[1;36m";
    const std::string bold_yellow = "\033[1;33m";
    const std::string bold_green  = "\033[1;32m";
    const std::string bold_red    = "\033[1;31m";
    const std::string bg_black    = "\033[40m";
}

std::string colorize(const std::string& text, const std::string& color) {
    return color + text + ansi::reset;
}

// Framework color map
std::string framework_color(const std::string& framework) {
    static const std::unordered_map<std::string, std::string> colors = {
        {"Next.js",        ansi::bold_white + ansi::bg_black},
        {"Vite",           ansi::yellow},
        {"React",          ansi::cyan},
        {"Vue",            ansi::green},
        {"Angular",        ansi::red},
        {"Svelte",         "\033[38;2;255;62;0m"},
        {"SvelteKit",      "\033[38;2;255;62;0m"},
        {"Express",        ansi::gray},
        {"Fastify",        ansi::white},
        {"NestJS",         ansi::red},
        {"Nuxt",           ansi::green},
        {"Remix",          ansi::blue},
        {"Astro",          ansi::magenta},
        {"Django",         ansi::green},
        {"Flask",          ansi::white},
        {"FastAPI",        ansi::cyan},
        {"Rails",          ansi::red},
        {"Gatsby",         ansi::magenta},
        {"Go",             ansi::cyan},
        {"Rust",           "\033[38;2;222;165;93m"},
        {"Ruby",           ansi::red},
        {"Python",         ansi::yellow},
        {"Node.js",        ansi::green},
        {"Java",           ansi::red},
        {"Hono",           "\033[38;2;255;102;0m"},
        {"Koa",            ansi::white},
        {"Webpack",        ansi::blue},
        {"esbuild",        ansi::yellow},
        {"Parcel",         "\033[38;2;224;178;77m"},
        {"Docker",         ansi::blue},
        {"PostgreSQL",     ansi::blue},
        {"Redis",          ansi::red},
        {"MySQL",          ansi::blue},
        {"MongoDB",        ansi::green},
        {"nginx",          ansi::green},
        {"LocalStack",     ansi::white},
        {"RabbitMQ",       "\033[38;2;255;102;0m"},
        {"Kafka",          ansi::white},
        {"Elasticsearch",  ansi::yellow},
        {"MinIO",          ansi::red},
    };

    auto it = colors.find(framework);
    if (it != colors.end()) {
        return it->second + framework + ansi::reset;
    }
    return framework;
}

std::string format_framework(const std::optional<std::string>& framework) {
    if (!framework) return colorize("\xe2\x80\x94", ansi::gray);
    return framework_color(*framework);
}

std::string format_status(const std::string& status) {
    if (status == "healthy") {
        return colorize("\xe2\x97\x8f", ansi::green) + " " + colorize("healthy", ansi::green);
    } else if (status == "orphaned") {
        return colorize("\xe2\x97\x8f", ansi::yellow) + " " + colorize("orphaned", ansi::yellow);
    } else if (status == "zombie") {
        return colorize("\xe2\x97\x8f", ansi::red) + " " + colorize("zombie", ansi::red);
    }
    return colorize("\xe2\x97\x8f", ansi::gray) + " " + colorize("unknown", ansi::gray);
}

std::string dash() {
    return colorize("\xe2\x80\x94", ansi::gray);
}

void render_header() {
    std::string line1 = "  \xf0\x9f\x94\x8a Port Whisperer";
    std::string line2 = "  listening to your ports...";
    std::string border_line;
    for (int i = 0; i < 37; ++i) border_line += "\xe2\x94\x80";

    std::cout << "\n";
    std::cout << colorize(" \xe2\x94\x8c" + border_line + "\xe2\x94\x90", ansi::bold_cyan) << "\n";
    std::cout << colorize(" \xe2\x94\x82", ansi::bold_cyan)
              << colorize(line1, ansi::bold_white);
    // Pad to box width (approximate)
    int pad = 37 - 20; // rough estimate
    for (int i = 0; i < pad; ++i) std::cout << " ";
    std::cout << colorize("\xe2\x94\x82", ansi::bold_cyan) << "\n";

    std::cout << colorize(" \xe2\x94\x82", ansi::bold_cyan)
              << colorize(line2, ansi::gray);
    pad = 37 - 28;
    for (int i = 0; i < pad; ++i) std::cout << " ";
    std::cout << colorize("\xe2\x94\x82", ansi::bold_cyan) << "\n";
    std::cout << colorize(" \xe2\x94\x94" + border_line + "\xe2\x94\x98", ansi::bold_cyan) << "\n";
    std::cout << "\n";
}

} // anonymous namespace

void display_port_table(const std::vector<PortInfo>& ports, bool filtered) {
    render_header();

    if (ports.empty()) {
        std::cout << colorize("  No active listening ports found.\n", ansi::gray) << "\n";
        std::cout << colorize("  Start a dev server and run ", ansi::gray)
                  << colorize("ports", ansi::cyan)
                  << colorize(" again.\n", ansi::gray) << "\n";
        return;
    }

    tabulate::Table table;
    table.add_row({"PORT", "PROCESS", "PID", "PROJECT", "FRAMEWORK", "UPTIME", "STATUS"});

    // Style header
    for (size_t i = 0; i < 7; ++i) {
        table[0][i].format()
            .font_color(tabulate::Color::cyan)
            .font_style({tabulate::FontStyle::bold});
    }

    for (const auto& p : ports) {
        table.add_row({
            ":" + std::to_string(p.port),
            p.process_name.empty() ? "\xe2\x80\x94" : p.process_name,
            std::to_string(p.pid),
            p.project_name ? truncate(*p.project_name, 20) : "\xe2\x80\x94",
            p.framework.value_or("\xe2\x80\x94"),
            p.uptime.value_or("\xe2\x80\x94"),
            p.status
        });
    }

    // Style the table with Unicode box-drawing characters
    table.format()
        .border_top("\xe2\x94\x80")
        .border_bottom("\xe2\x94\x80")
        .border_left("\xe2\x94\x82")
        .border_right("\xe2\x94\x82")
        .corner("\xe2\x94\xbc")
        .corner_top_left("\xe2\x94\x8c")
        .corner_top_right("\xe2\x94\x90")
        .corner_bottom_left("\xe2\x94\x94")
        .corner_bottom_right("\xe2\x94\x98")
        .border_color(tabulate::Color::grey);

    // First row: proper Unicode corners and borders
    for (size_t i = 0; i < 7; ++i) {
        table[0][i].format()
            .border_top("\xe2\x94\x80")
            .border_bottom("\xe2\x94\x80")
            .corner_top_left(i == 0 ? "\xe2\x94\x8c" : "\xe2\x94\xac")
            .corner_top_right(i == 6 ? "\xe2\x94\x90" : "\xe2\x94\xac")
            .corner_bottom_left(i == 0 ? "\xe2\x94\x9c" : "\xe2\x94\xbc")
            .corner_bottom_right(i == 6 ? "\xe2\x94\xa4" : "\xe2\x94\xbc");
    }

    // Last row: use ┴ for bottom-mid corners
    size_t last = table.size() - 1;
    if (last > 0) {
        for (size_t i = 0; i < 7; ++i) {
            table[last][i].format()
                .corner_bottom_left(i == 0 ? "\xe2\x94\x94" : "\xe2\x94\xb4")
                .corner_bottom_right(i == 6 ? "\xe2\x94\x98" : "\xe2\x94\xb4");
        }
    }

    // Row 1 (first data row): fix top corners to ├/┼/┤
    if (last >= 1) {
        for (size_t i = 0; i < 7; ++i) {
            table[1][i].format()
                .border_top("\xe2\x94\x80")
                .corner_top_left(i == 0 ? "\xe2\x94\x9c" : "\xe2\x94\xbc")
                .corner_top_right(i == 6 ? "\xe2\x94\xa4" : "\xe2\x94\xbc");
        }
    }

    // Hide internal row separators between data rows (rows 2+)
    for (size_t r = 2; r <= last; ++r) {
        for (size_t i = 0; i < 7; ++i) {
            table[r][i].format()
                .border_top("")
                .corner_top_left("")
                .corner_top_right("");
        }
    }

    std::cout << table << "\n\n";

    std::string summary = colorize(
        fmt::format("  {} port{} active  \xc2\xb7  ", ports.size(), ports.size() == 1 ? "" : "s"),
        ansi::gray);
    summary += colorize("Run ", ansi::gray) + colorize("ports <number>", ansi::cyan) + colorize(" for details", ansi::gray);
    if (filtered) {
        summary += colorize("  \xc2\xb7  ", ansi::gray) + colorize("--all", ansi::cyan) + colorize(" to show everything", ansi::gray);
    }
    std::cout << summary << "\n\n";
}

void display_process_table(const std::vector<ProcessInfo>& processes, bool filtered) {
    render_header();

    if (processes.empty()) {
        std::cout << colorize("  No dev processes found.\n", ansi::gray) << "\n";
        std::cout << colorize("  Run ", ansi::gray) << colorize("ports ps --all", ansi::cyan)
                  << colorize(" to show all processes.\n", ansi::gray) << "\n";
        return;
    }

    tabulate::Table table;
    table.add_row({"PID", "PROCESS", "CPU%", "MEM", "PROJECT", "FRAMEWORK", "UPTIME", "WHAT"});

    for (size_t i = 0; i < 8; ++i) {
        table[0][i].format()
            .font_color(tabulate::Color::cyan)
            .font_style({tabulate::FontStyle::bold});
    }

    for (const auto& p : processes) {
        std::string cpu_str = fmt::format("{:.1f}", p.cpu);

        table.add_row({
            std::to_string(p.pid),
            truncate(p.process_name, 15),
            cpu_str,
            p.memory.value_or("\xe2\x80\x94"),
            p.project_name ? truncate(*p.project_name, 20) : "\xe2\x80\x94",
            p.framework.value_or("\xe2\x80\x94"),
            p.uptime.value_or("\xe2\x80\x94"),
            truncate(p.description.empty() ? p.process_name : p.description, 30)
        });
    }

    // Style with Unicode box-drawing
    size_t ncols = 8;
    table.format()
        .border_top("\xe2\x94\x80")
        .border_bottom("\xe2\x94\x80")
        .border_left("\xe2\x94\x82")
        .border_right("\xe2\x94\x82")
        .corner("\xe2\x94\xbc")
        .corner_top_left("\xe2\x94\x8c")
        .corner_top_right("\xe2\x94\x90")
        .corner_bottom_left("\xe2\x94\x94")
        .corner_bottom_right("\xe2\x94\x98")
        .border_color(tabulate::Color::grey);

    for (size_t i = 0; i < ncols; ++i) {
        table[0][i].format()
            .border_top("\xe2\x94\x80")
            .border_bottom("\xe2\x94\x80")
            .corner_top_left(i == 0 ? "\xe2\x94\x8c" : "\xe2\x94\xac")
            .corner_top_right(i == ncols-1 ? "\xe2\x94\x90" : "\xe2\x94\xac")
            .corner_bottom_left(i == 0 ? "\xe2\x94\x9c" : "\xe2\x94\xbc")
            .corner_bottom_right(i == ncols-1 ? "\xe2\x94\xa4" : "\xe2\x94\xbc");
    }
    size_t last_ps = table.size() - 1;
    if (last_ps > 0) {
        for (size_t i = 0; i < ncols; ++i) {
            table[last_ps][i].format()
                .corner_bottom_left(i == 0 ? "\xe2\x94\x94" : "\xe2\x94\xb4")
                .corner_bottom_right(i == ncols-1 ? "\xe2\x94\x98" : "\xe2\x94\xb4");
        }
    }
    // First data row: ├/┼/┤ separator
    if (last_ps >= 1) {
        for (size_t i = 0; i < ncols; ++i) {
            table[1][i].format()
                .border_top("\xe2\x94\x80")
                .corner_top_left(i == 0 ? "\xe2\x94\x9c" : "\xe2\x94\xbc")
                .corner_top_right(i == ncols-1 ? "\xe2\x94\xa4" : "\xe2\x94\xbc");
        }
    }
    for (size_t r = 2; r <= last_ps; ++r) {
        for (size_t i = 0; i < ncols; ++i) {
            table[r][i].format().border_top("").corner_top_left("").corner_top_right("");
        }
    }

    std::cout << table << "\n\n";

    std::string summary = colorize(
        fmt::format("  {} process{}", processes.size(), processes.size() == 1 ? "" : "es"),
        ansi::gray);
    if (filtered) {
        summary += colorize("  \xc2\xb7  ", ansi::gray) + colorize("--all", ansi::cyan) + colorize(" to show everything", ansi::gray);
    }
    std::cout << summary << "\n\n";
}

void display_port_detail(const std::optional<PortInfo>& info) {
    render_header();

    if (!info) {
        std::cout << colorize("  No process found on that port.\n", ansi::red) << "\n";
        return;
    }

    auto box = [](const std::string& label, const std::string& value) {
        std::string padded = label;
        while (padded.size() < 16) padded += ' ';
        std::cout << "  " << colorize(padded, ansi::gray) << " " << value << "\n";
    };

    std::cout << colorize(fmt::format("  Port :{}", info->port), ansi::bold_white) << "\n";
    std::string sep;
    for (int i = 0; i < 22; ++i) sep += "\xe2\x94\x80";
    std::cout << colorize("  " + sep, ansi::gray) << "\n\n";

    box("Process", colorize(info->process_name.empty() ? "\xe2\x80\x94" : info->process_name, ansi::bold_white));
    box("PID", colorize(std::to_string(info->pid), ansi::gray));
    box("Status", format_status(info->status));
    box("Framework", format_framework(info->framework));
    box("Memory", info->memory ? colorize(*info->memory, ansi::green) : dash());
    box("Uptime", info->uptime ? colorize(*info->uptime, ansi::yellow) : dash());

    if (info->start_time) {
        auto time_t_val = std::chrono::system_clock::to_time_t(*info->start_time);
        std::string time_str = std::ctime(&time_t_val);
        if (!time_str.empty() && time_str.back() == '\n') time_str.pop_back();
        box("Started", colorize(time_str, ansi::gray));
    }

    std::cout << "\n" << colorize("  Location", ansi::bold_cyan) << "\n";
    std::cout << colorize("  " + sep, ansi::gray) << "\n";
    box("Directory", info->cwd ? colorize(*info->cwd, ansi::blue) : dash());
    box("Project", info->project_name ? colorize(*info->project_name, ansi::white) : dash());
    box("Git Branch", info->git_branch ? colorize(*info->git_branch, ansi::magenta) : dash());

    if (!info->process_tree.empty()) {
        std::cout << "\n" << colorize("  Process Tree", ansi::bold_cyan) << "\n";
        std::cout << colorize("  " + sep, ansi::gray) << "\n";
        for (size_t i = 0; i < info->process_tree.size(); ++i) {
            const auto& node = info->process_tree[i];
            std::string indent(i * 2, ' ');
            std::string prefix = (i == 0) ? "\xe2\x86\x92" : "\xe2\x94\x94\xe2\x94\x80";
            std::string name_color = (node.pid == info->pid) ? ansi::bold_white : ansi::gray;
            std::cout << "  " << indent
                      << colorize(prefix, ansi::gray) << " "
                      << colorize(node.name, name_color) << " "
                      << colorize(fmt::format("({})", node.pid), ansi::gray) << "\n";
        }
    }

    std::cout << "\n"
              << colorize(fmt::format("  Kill: ", info->pid), ansi::gray)
              << colorize(fmt::format("ports kill {}", info->port), ansi::cyan)
              << "\n\n";
}

void display_clean_results(const std::vector<PortInfo>& orphaned,
                           const std::vector<pid_t>& killed,
                           const std::vector<pid_t>& failed) {
    render_header();

    if (orphaned.empty()) {
        std::cout << colorize("  \xe2\x9c\x93 No orphaned or zombie processes found. All clean!\n", ansi::green) << "\n";
        return;
    }

    std::cout << colorize(
        fmt::format("  Found {} orphaned/zombie process{}:\n",
                     orphaned.size(), orphaned.size() == 1 ? "" : "es"),
        ansi::bold_yellow) << "\n";

    for (const auto& p : orphaned) {
        bool was_killed = std::find(killed.begin(), killed.end(), p.pid) != killed.end();
        bool did_fail = std::find(failed.begin(), failed.end(), p.pid) != failed.end();
        std::string icon = was_killed ? colorize("\xe2\x9c\x93", ansi::green)
                         : did_fail   ? colorize("\xe2\x9c\x95", ansi::red)
                         :              colorize("?", ansi::yellow);

        std::cout << "  " << icon << " :"
                  << colorize(std::to_string(p.port), ansi::bold_white) << " "
                  << colorize("\xe2\x80\x94", ansi::gray) << " "
                  << p.process_name << " "
                  << colorize(fmt::format("(PID {})", p.pid), ansi::gray) << "\n";

        if (did_fail) {
            std::cout << colorize(fmt::format("    Failed to kill. Try: sudo kill -9 {}", p.pid), ansi::red) << "\n";
        }
    }

    std::cout << "\n";
    if (!killed.empty()) {
        std::cout << colorize(
            fmt::format("  Cleaned {} process{}.", killed.size(), killed.size() == 1 ? "" : "es"),
            ansi::green) << "\n";
    }
    if (!failed.empty()) {
        std::cout << colorize(
            fmt::format("  Failed to clean {} process{}.", failed.size(), failed.size() == 1 ? "" : "es"),
            ansi::red) << "\n";
    }
    std::cout << "\n";
}

void display_watch_header() {
    render_header();
    std::cout << colorize("  Watching for port changes...", ansi::bold_cyan) << "\n";
    std::cout << colorize("  Press Ctrl+C to stop\n", ansi::gray) << "\n";
}

void display_watch_event(std::string_view type, const PortInfo& info) {
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    std::tm tm_val;
    localtime_r(&time_t_val, &tm_val);
    std::string timestamp = fmt::format("{:02d}:{:02d}:{:02d}", tm_val.tm_hour, tm_val.tm_min, tm_val.tm_sec);

    if (type == "new") {
        std::string fw = info.framework ? " " + framework_color(*info.framework) : "";
        std::string proj = info.project_name ? colorize(" [" + *info.project_name + "]", ansi::blue) : "";
        std::cout << "  " << colorize(timestamp, ansi::gray) << " "
                  << colorize("\xe2\x96\xb2 NEW", ansi::green) << "    :"
                  << colorize(std::to_string(info.port), ansi::bold_white) << " \xe2\x86\x90 "
                  << colorize(info.process_name, ansi::white)
                  << proj << fw << "\n";
    } else if (type == "removed") {
        std::cout << "  " << colorize(timestamp, ansi::gray) << " "
                  << colorize("\xe2\x96\xbc CLOSED", ansi::red) << " :"
                  << colorize(std::to_string(info.port), ansi::bold_white) << "\n";
    }
}

void display_help() {
    std::cout << "\n"
              << colorize("  Port Whisperer", ansi::bold_cyan)
              << colorize(" \xe2\x80\x94 listen to your ports", ansi::gray) << "\n\n"
              << colorize("  Usage:", ansi::white) << "\n"
              << "    " << colorize("ports", ansi::cyan) << "              Show dev server ports\n"
              << "    " << colorize("ports --all", ansi::cyan) << "        Show all listening ports\n"
              << "    " << colorize("ports ps", ansi::cyan) << "           Show all running dev processes\n"
              << "    " << colorize("ports ps --all", ansi::cyan) << "    Show all processes\n"
              << "    " << colorize("ports <number>", ansi::cyan) << "     Detailed info about a specific port\n"
              << "    " << colorize("ports kill <n>", ansi::cyan) << "     Kill by port or PID (-f for SIGKILL)\n"
              << "    " << colorize("ports clean", ansi::cyan) << "        Kill orphaned/zombie dev servers\n"
              << "    " << colorize("ports watch", ansi::cyan) << "        Monitor port changes in real-time\n"
              << "    " << colorize("whoisonport <num>", ansi::cyan) << " Alias for ports <number>\n"
              << "\n";
}

} // namespace pw
