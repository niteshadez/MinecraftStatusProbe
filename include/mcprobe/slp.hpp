#ifndef SLP_HPP
#define SLP_HPP

#include <string>
#include <vector>
#include <fstream>
#include <mutex>

// Returns the name of the matched target player if found, or an empty string otherwise.
std::string queryMinecraftServer(const std::string& host, int port, const std::vector<std::string>& targetPlayers);

#endif