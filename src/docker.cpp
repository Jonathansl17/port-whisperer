#include "docker.hpp"
#include "util.hpp"

#include <regex>
#include <set>
#include <sstream>

namespace pw {

std::map<uint16_t, DockerContainer> batch_docker_info() {
    std::map<uint16_t, DockerContainer> result;

    std::string raw = exec_command(
        R"(docker ps --format "{{.Ports}}\t{{.Names}}\t{{.Image}}")");
    if (raw.empty()) return result;

    static const std::regex port_re(R"((?:\d+\.\d+\.\d+\.\d+|::):(\d+)->)");

    std::istringstream iss(raw);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;

        // Split by tab: ports, name, image
        auto tab1 = line.find('\t');
        if (tab1 == std::string::npos) continue;
        auto tab2 = line.find('\t', tab1 + 1);
        if (tab2 == std::string::npos) continue;

        std::string ports_str = line.substr(0, tab1);
        std::string name = line.substr(tab1 + 1, tab2 - tab1 - 1);
        std::string image = line.substr(tab2 + 1);

        if (name.empty()) continue;

        // Parse port mappings
        std::set<uint16_t> seen;
        auto begin = std::sregex_iterator(ports_str.begin(), ports_str.end(), port_re);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            uint16_t port = static_cast<uint16_t>(std::stoi((*it)[1]));
            if (seen.count(port)) continue;
            seen.insert(port);
            result[port] = DockerContainer{.name = name, .image = image};
        }
    }

    return result;
}

} // namespace pw
