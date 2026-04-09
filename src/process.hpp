#pragma once

#include "models.hpp"

#include <map>
#include <optional>
#include <vector>

namespace pw {

// Batch-fetch ps info for multiple PIDs in one call
std::map<pid_t, PsEntry> batch_ps_info(const std::vector<pid_t>& pids);

// Get process tree walking upward from pid
std::vector<ProcessTreeNode> get_process_tree(pid_t pid);

// Get all running processes (for `ports ps`)
std::vector<ProcessInfo> get_all_processes();

// Check if a PID exists
bool pid_exists(pid_t pid);

// Send signal to a process
bool kill_process(pid_t pid, KillSignal sig = KillSignal::Term);

// Resolve a number to a kill target (port-first, then PID)
std::optional<KillTarget> resolve_kill_target(int n);

} // namespace pw
