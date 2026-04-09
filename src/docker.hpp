#pragma once

#include "models.hpp"

#include <cstdint>
#include <map>

namespace pw {

// Fetch docker container info mapped by host port
std::map<uint16_t, DockerContainer> batch_docker_info();

} // namespace pw
