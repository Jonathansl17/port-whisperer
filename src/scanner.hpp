#pragma once

#include "models.hpp"

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace pw {

// Get all listening ports with process info. detailed=true adds git branch + process tree.
std::vector<PortInfo> get_listening_ports(bool detailed = false);

// Get detailed info for a specific port
std::optional<PortInfo> get_port_details(uint16_t target_port);

// Find orphaned/zombie dev server processes
std::vector<PortInfo> find_orphaned_processes();

// Watch for port changes, calling callback on new/removed ports.
// Blocks until interrupted. Returns when callback returns false or signal received.
using WatchCallback = std::function<void(std::string_view type, const PortInfo& info)>;
void watch_ports(WatchCallback callback, int interval_ms = 2000);

} // namespace pw
