#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace mcprobe {

struct Endpoint {
    std::string address;
    std::uint16_t port{};

    [[nodiscard]] std::string to_string() const;
};

[[nodiscard]] std::vector<Endpoint> load_targets(const std::filesystem::path& path);
[[nodiscard]] std::vector<std::string> load_player_names(const std::filesystem::path& path);

} // namespace mcprobe
