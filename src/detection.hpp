#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace pw {

// Detect framework from command line string (fastest, layer 1)
std::optional<std::string> detect_framework_from_command(const std::string& command, const std::string& process_name);

// Detect framework from process name only (fallback)
std::optional<std::string> detect_framework_from_name(const std::string& process_name);

// Detect framework from project root (package.json deps + config files, layer 2+3)
std::optional<std::string> detect_framework(const std::string& project_root);

// Detect framework/service from Docker image name
std::string detect_framework_from_image(const std::string& image);

// Check if process looks like a dev server vs system app
bool is_dev_process(const std::string& process_name, const std::string& command);

} // namespace pw
