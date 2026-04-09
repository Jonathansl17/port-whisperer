#pragma once

#include "../models.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace pw::platform {

// Get all TCP sockets in LISTEN state -> (port, pid, process_name)
std::vector<RawPortEntry> get_listening_ports();

// Get cwd for a single pid
std::optional<std::string> get_process_cwd(pid_t pid);

// Batch cwd lookup for multiple pids
std::map<pid_t, std::string> batch_get_cwd(const std::vector<pid_t>& pids);

} // namespace pw::platform
