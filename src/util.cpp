#include "util.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <vector>

#include <fmt/format.h>

namespace fs = std::filesystem;

namespace pw {

std::string exec_command(const std::string& cmd, int /*timeout_ms*/) {
    std::string result;
    std::string full_cmd = cmd + " 2>/dev/null";

    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) return result;

    std::array<char, 4096> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);

    // Trim trailing whitespace
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
        result.pop_back();
    }
    return result;
}

std::string format_memory(uint64_t rss_kb) {
    if (rss_kb > 1048576) {
        return fmt::format("{:.1f} GB", static_cast<double>(rss_kb) / 1048576.0);
    }
    if (rss_kb > 1024) {
        return fmt::format("{:.1f} MB", static_cast<double>(rss_kb) / 1024.0);
    }
    return fmt::format("{} KB", rss_kb);
}

std::string format_uptime(int64_t ms) {
    int64_t seconds = ms / 1000;
    int64_t minutes = seconds / 60;
    int64_t hours = minutes / 60;
    int64_t days = hours / 24;

    if (days > 0) return fmt::format("{}d {}h", days, hours % 24);
    if (hours > 0) return fmt::format("{}h {}m", hours, minutes % 60);
    if (minutes > 0) return fmt::format("{}m {}s", minutes, seconds % 60);
    return fmt::format("{}s", seconds);
}

std::string truncate(std::string_view str, size_t max) {
    if (str.empty()) return "";
    if (str.size() <= max) return std::string(str);
    return std::string(str.substr(0, max - 1)) + "\xe2\x80\xa6"; // UTF-8 ellipsis
}

std::string find_project_root(const std::string& dir) {
    static constexpr std::array markers = {
        "package.json", "Cargo.toml", "go.mod",
        "pyproject.toml", "Gemfile", "pom.xml", "build.gradle"
    };

    fs::path current(dir);
    int depth = 0;
    while (current != current.root_path() && depth < 15) {
        for (const auto& marker : markers) {
            if (fs::exists(current / marker)) {
                return current.string();
            }
        }
        current = current.parent_path();
        ++depth;
    }
    return dir;
}

std::string path_basename(const std::string& path) {
    return fs::path(path).filename().string();
}

std::string summarize_command(const std::string& command, const std::string& process_name) {
    if (command.empty()) return process_name;

    std::istringstream iss(command);
    std::string part;
    std::vector<std::string> meaningful;

    int i = 0;
    while (iss >> part && meaningful.size() < 3) {
        if (i++ == 0) continue;       // Skip the binary path
        if (part[0] == '-') continue;  // Skip flags
        if (part.find('/') != std::string::npos) {
            meaningful.push_back(path_basename(part));
        } else {
            meaningful.push_back(part);
        }
    }

    if (!meaningful.empty()) {
        std::string result;
        for (size_t j = 0; j < meaningful.size(); ++j) {
            if (j > 0) result += ' ';
            result += meaningful[j];
        }
        return result;
    }

    return process_name;
}

} // namespace pw
