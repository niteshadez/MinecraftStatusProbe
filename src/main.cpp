#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <windows.h>
#include <shlobj.h>
#include "mcprobe/slp.hpp"
#include "mcprobe/gui.hpp"

bool isServerWhitelisted(const std::string& host, int port);

std::filesystem::path getConfigFilePath(const std::string& filename) {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::filesystem::path dir = std::filesystem::path(path) / "MinecraftStatusProbe";
        std::filesystem::create_directories(dir);
        return dir / filename;
    }
    return filename;
}

std::string trimString(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

void loadSavedConfig(std::string& rawEndpoints, std::string& rawPlayers) {
    auto tPath = getConfigFilePath("targets.md");
    auto pPath = getConfigFilePath("players.md");

    if (std::filesystem::exists(tPath)) {
        std::ifstream file(tPath);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            rawEndpoints = buffer.str();
        }
    }

    if (std::filesystem::exists(pPath)) {
        std::ifstream file(pPath);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            rawPlayers = buffer.str();
        }
    }
}

void saveCurrentConfig(const std::string& rawEndpoints, const std::string& rawPlayers) {
    auto tPath = getConfigFilePath("targets.md");
    auto pPath = getConfigFilePath("players.md");

    std::ofstream tFile(tPath, std::ios::trunc);
    if (tFile.is_open()) {
        tFile << rawEndpoints;
    }

    std::ofstream pFile(pPath, std::ios::trunc);
    if (pFile.is_open()) {
        pFile << rawPlayers;
    }
}

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    std::atomic<bool> isRunning(true);
    std::atomic<bool> triggerScan(false);
    std::string rawEndpoints;
    std::string rawPlayers;
    std::mutex dataMutex;
    std::mutex logMutex;

    loadSavedConfig(rawEndpoints, rawPlayers);

    std::thread backgroundWorker([&]() {
        while (isRunning) {
            if (triggerScan.load()) {
                triggerScan.store(false);

                std::string localEndpoints, localPlayers;
                {
                    std::lock_guard<std::mutex> lock(dataMutex);
                    localEndpoints = rawEndpoints;
                    localPlayers = rawPlayers;
                }

                saveCurrentConfig(localEndpoints, localPlayers);

                std::vector<std::string> targetPlayers;
                {
                    std::stringstream pStream(localPlayers);
                    std::string pLine;
                    while (std::getline(pStream, pLine)) {
                        std::string cleaned = trimString(pLine);
                        if (!cleaned.empty() && cleaned[0] != '#' && cleaned[0] != '(') {
                            targetPlayers.push_back(cleaned);
                        }
                    }
                }

                struct Endpoint { std::string ip; int port; };
                std::vector<Endpoint> endpoints;
                {
                    std::stringstream eStream(localEndpoints);
                    std::string eLine;
                    while (std::getline(eStream, eLine)) {
                        std::string cleaned = trimString(eLine);
                        if (cleaned.empty() || cleaned[0] == '#' || cleaned[0] == '(') {
                            continue;
                        }

                        size_t colon = cleaned.find(':');
                        if (colon != std::string::npos) {
                            std::string ip = trimString(cleaned.substr(0, colon));
                            if (!ip.empty() && ip.find('.') != std::string::npos) {
                                try {
                                    std::string portStr = trimString(cleaned.substr(colon + 1));
                                    int port = std::stoi(portStr);
                                    endpoints.push_back({ip, port});
                                } catch (...) {}
                            }
                        }
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(logMutex);
                    AppendTextToLog(hEditLog, "[*] Scanning " + std::to_string(endpoints.size()) + " valid endpoints for " + std::to_string(targetPlayers.size()) + " target players...");
                }

                std::atomic<int> onlineServersCount(0);
                std::atomic<int> whitelistedServersCount(0);
                std::atomic<int> playerMatchesCount(0);
                std::atomic<int> whitelistedPlayerMatchesCount(0);

                const size_t maxThreads = 32;
                std::vector<std::jthread> pool;

                for (const auto& ep : endpoints) {
                    if (!isRunning) break;
                    if (pool.size() >= maxThreads) {
                        pool.clear();
                    }

                    pool.emplace_back([ip = ep.ip, port = ep.port, &targetPlayers, &isRunning, &logMutex,
                                       &onlineServersCount, &whitelistedServersCount, &playerMatchesCount, &whitelistedPlayerMatchesCount]() {
                        if (!isRunning) return;

                        std::string epStr = ip + ":" + std::to_string(port);
                        {
                            std::lock_guard<std::mutex> lock(logMutex);
                            AppendTextToLog(hEditLog, "[>] Checking: " + epStr);
                        }

                        std::string result = queryMinecraftServer(ip, port, targetPlayers);

                        if (!result.empty() && result != "ONLINE_ONLY" && result != "UNREACHABLE") {
                            if (isServerWhitelisted(ip, port)) {
                                whitelistedPlayerMatchesCount.fetch_add(1);
                                std::lock_guard<std::mutex> lock(logMutex);
                                AppendTextToLog(hEditLog, "[+] Whitelisted Player Match Found: " + epStr + " (Target: " + result + ")");
                            } else {
                                playerMatchesCount.fetch_add(1);
                                std::lock_guard<std::mutex> lock(logMutex);
                                AppendTextToLog(hEditLog, "[+] Player Match Found: " + epStr + " (Target: " + result + ")");
                            }
                        } else if (result == "ONLINE_ONLY") {
                            if (isServerWhitelisted(ip, port)) {
                                whitelistedServersCount.fetch_add(1);
                                std::lock_guard<std::mutex> lock(logMutex);
                                AppendTextToLog(hEditLog, "[*] Whitelisted Server found: " + epStr);
                            } else {
                                onlineServersCount.fetch_add(1);
                                std::lock_guard<std::mutex> lock(logMutex);
                                AppendTextToLog(hEditLog, "[*] Online Server (No Target Match): " + epStr);
                            }
                        } else if (result == "UNREACHABLE") {
                            std::lock_guard<std::mutex> lock(logMutex);
                            AppendTextToLog(hEditLog, "[!] Unable to be reached: " + epStr);
                        } else {
                            if (isServerWhitelisted(ip, port)) {
                                whitelistedServersCount.fetch_add(1);
                                std::lock_guard<std::mutex> lock(logMutex);
                                AppendTextToLog(hEditLog, "[*] Whitelisted Server found: " + epStr);
                            } else {
                                std::lock_guard<std::mutex> lock(logMutex);
                                AppendTextToLog(hEditLog, "[-] Offline: " + epStr);
                            }
                        }
                    });
                }
                pool.clear();

                // Build summary string in exact requested format
                int online = onlineServersCount.load();
                int whitelist = whitelistedServersCount.load();
                int matches = playerMatchesCount.load();
                int whiteMatches = whitelistedPlayerMatchesCount.load();

                std::string summary = std::to_string(online) + (online == 1 ? " Online server found, " : " Online servers found, ") +
                                      std::to_string(whitelist) + (whitelist == 1 ? " Whitelisted server, " : " Whitelisted servers, ") +
                                      std::to_string(matches) + (matches == 1 ? " Player match, and " : " Player matches, and ") +
                                      std::to_string(whiteMatches) + (whiteMatches == 1 ? " Whitelisted player match found." : " Whitelisted player matches found.");

                {
                    std::lock_guard<std::mutex> lock(logMutex);
                    AppendTextToLog(hEditLog, "[*] Scan complete! " + summary);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    launchGuiApp(isRunning, triggerScan, rawEndpoints, rawPlayers, dataMutex);

    {
        std::lock_guard<std::mutex> lock(dataMutex);
        saveCurrentConfig(rawEndpoints, rawPlayers);
    }

    isRunning = false;
    if (backgroundWorker.joinable()) {
        backgroundWorker.join();
    }

    WSACleanup();
    return 0;
}