#include "mcprobe/target.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <stdexcept>
#include <string_view>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace mcprobe {
namespace {

std::string_view trim(std::string_view value) {
    constexpr std::string_view whitespace = " \t\r\n";
    const auto first = value.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    return value.substr(first, value.find_last_not_of(whitespace) - first + 1);
}

bool is_ip_address(std::string_view address) {
    const std::string text(address);
    in_addr ipv4{};
    in6_addr ipv6{};
    return inet_pton(AF_INET, text.c_str(), &ipv4) == 1 ||
           inet_pton(AF_INET6, text.c_str(), &ipv6) == 1;
}

Endpoint parse_endpoint(std::string_view line, std::size_t line_number) {
    std::string_view address;
    std::string_view port_text;

    if (!line.empty() && line.front() == '[') {
        const auto close = line.find(']');
        if (close == std::string_view::npos || close + 1 >= line.size() ||
            line[close + 1] != ':') {
            throw std::runtime_error("invalid endpoint on line " + std::to_string(line_number));
        }
        address = line.substr(1, close - 1);
        port_text = line.substr(close + 2);
    } else {
        const auto separator = line.rfind(':');
        if (separator == std::string_view::npos ||
            line.find(':') != separator) {
            throw std::runtime_error("expected IPv4:port or [IPv6]:port on line " +
                                     std::to_string(line_number));
        }
        address = line.substr(0, separator);
        port_text = line.substr(separator + 1);
    }

    if (!is_ip_address(address)) {
        throw std::runtime_error("expected a literal IP address on line " +
                                 std::to_string(line_number));
    }

    unsigned int port = 0;
    const auto [end, error] = std::from_chars(
        port_text.data(), port_text.data() + port_text.size(), port);
    if (error != std::errc{} || end != port_text.data() + port_text.size() ||
        port == 0 || port > 65535) {
        throw std::runtime_error("invalid port on line " + std::to_string(line_number));
    }

    return {std::string(address), static_cast<std::uint16_t>(port)};
}

std::vector<std::string> load_lines(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open " + path.string());
    }

    std::vector<std::string> values;
    std::string line;
    while (std::getline(input, line)) {
        const auto value = trim(line);
        if (value.empty() || value.front() == '#') {
            continue;
        }
        values.emplace_back(value);
    }
    if (input.bad()) {
        throw std::runtime_error("failed while reading " + path.string());
    }
    return values;
}

} // namespace

std::string Endpoint::to_string() const {
    if (address.find(':') != std::string::npos) {
        return "[" + address + "]:" + std::to_string(port);
    }
    return address + ":" + std::to_string(port);
}

std::vector<Endpoint> load_targets(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open " + path.string());
    }

    std::vector<Endpoint> targets;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        const auto value = trim(line);
        if (value.empty() || value.front() == '#') {
            continue;
        }
        targets.push_back(parse_endpoint(value, line_number));
    }
    if (input.bad()) {
        throw std::runtime_error("failed while reading " + path.string());
    }
    if (targets.empty()) {
        throw std::runtime_error("no endpoints found in " + path.string());
    }
    return targets;
}

std::vector<std::string> load_player_names(const std::filesystem::path& path) {
    return load_lines(path);
}

} // namespace mcprobe
