#pragma once

#include "models.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace pw {

void display_port_table(const std::vector<PortInfo>& ports, bool filtered);
void display_process_table(const std::vector<ProcessInfo>& processes, bool filtered);
void display_port_detail(const std::optional<PortInfo>& info);
void display_clean_results(const std::vector<PortInfo>& orphaned,
                           const std::vector<pid_t>& killed,
                           const std::vector<pid_t>& failed);
void display_watch_header();
void display_watch_event(std::string_view type, const PortInfo& info);
void display_help();

} // namespace pw
