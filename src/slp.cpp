#include "mcprobe/slp.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

static void writeVarInt(std::vector<uint8_t>& buffer, int value) {
    while (true) {
        if ((value & ~0x7F) == 0) {
            buffer.push_back(static_cast<uint8_t>(value));
            return;
        }
        buffer.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
        value >>= 7;
    }
}

static void writeString(std::vector<uint8_t>& buffer, const std::string& str) {
    writeVarInt(buffer, static_cast<int>(str.length()));
    buffer.insert(buffer.end(), str.begin(), str.end());
}

static bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char c1, char c2) {
        return std::tolower(c1) == std::tolower(c2);
    });
}

bool isServerWhitelisted(const std::string& host, int port) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return false;
    }

#ifdef _WIN32
    DWORD timeout = 2500;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#endif

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        closesocket(sock);
        return false;
    }

    if (connect(sock, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(sock);
        return false;
    }

    std::vector<uint8_t> handshakeData;
    writeVarInt(handshakeData, 0x00);
    writeVarInt(handshakeData, 767);
    writeString(handshakeData, host);

    uint16_t netPort = htons(port);
    handshakeData.push_back((netPort >> 8) & 0xFF);
    handshakeData.push_back(netPort & 0xFF);
    writeVarInt(handshakeData, 2);

    std::vector<uint8_t> packet;
    writeVarInt(packet, static_cast<int>(handshakeData.size()));
    packet.insert(packet.end(), handshakeData.begin(), handshakeData.end());

    std::vector<uint8_t> loginData;
    writeVarInt(loginData, 0x00);
    writeString(loginData, "ScanProbe123");

    std::vector<uint8_t> loginPacket;
    writeVarInt(loginPacket, static_cast<int>(loginData.size()));
    loginPacket.insert(loginPacket.end(), loginData.begin(), loginData.end());

    packet.insert(packet.end(), loginPacket.begin(), loginPacket.end());

    if (send(sock, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0) == SOCKET_ERROR) {
        closesocket(sock);
        return true;
    }

    std::vector<char> buffer(4096);
    int bytesReceived = recv(sock, buffer.data(), static_cast<int>(buffer.size()), 0);

    closesocket(sock);

    if (bytesReceived > 0) {
        std::string response(buffer.begin(), buffer.begin() + bytesReceived);
        std::transform(response.begin(), response.end(), response.begin(), ::tolower);

        if (response.find("white-listed") != std::string::npos ||
            response.find("whitelist") != std::string::npos ||
            response.find("not on the whitelist") != std::string::npos ||
            response.find("not white-listed") != std::string::npos) {
            return true;
        }
    }

    return true; // Active TCP socket passing handshake state on blocked/restricted servers
}

std::string queryMinecraftServer(const std::string& host, int port, const std::vector<std::string>& targetPlayers) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return "";
    }

#ifdef _WIN32
    DWORD timeout = 250;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#endif

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        closesocket(sock);
        return "";
    }

    if (connect(sock, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(sock);
        return "";
    }

    std::vector<uint8_t> handshakeData;
    writeVarInt(handshakeData, 0x00);
    writeVarInt(handshakeData, 754);
    writeString(handshakeData, host);

    uint16_t netPort = htons(port);
    handshakeData.push_back((netPort >> 8) & 0xFF);
    handshakeData.push_back(netPort & 0xFF);
    writeVarInt(handshakeData, 1);

    std::vector<uint8_t> packet;
    writeVarInt(packet, static_cast<int>(handshakeData.size()));
    packet.insert(packet.end(), handshakeData.begin(), handshakeData.end());

    send(sock, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0);

    uint8_t statusRequest[] = { 0x01, 0x00 };
    send(sock, reinterpret_cast<const char*>(packet.data()), sizeof(statusRequest), 0);

    std::vector<char> buffer(4096);
    int bytesReceived = recv(sock, buffer.data(), static_cast<int>(buffer.size()), 0);

    closesocket(sock);

    if (bytesReceived > 0) {
        try {
            std::string response(buffer.begin(), buffer.begin() + bytesReceived);
            size_t jsonStart = response.find('{');
            if (jsonStart != std::string::npos) {
                std::string jsonStr = response.substr(jsonStart);
                auto j = nlohmann::json::parse(jsonStr);

                if (j.contains("players") && j["players"].contains("sample")) {
                    for (const auto& player : j["players"]["sample"]) {
                        std::string name = player["name"].get<std::string>();
                        for (const auto& target : targetPlayers) {
                            if (iequals(name, target)) {
                                return name;
                            }
                        }
                    }
                }
                return "ONLINE_ONLY";
            }
        } catch (...) {}
    }
    return "";
}