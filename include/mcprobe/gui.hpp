#ifndef GUI_HPP
#define GUI_HPP

#include <windows.h>
#include <string>
#include <atomic>

extern HWND hEditLog;

void AppendTextToLog(HWND hEdit, const std::string& text);
void launchGuiApp(std::atomic<bool>& isRunning, std::atomic<bool>& startScan, std::string& outIps, std::string& outPlayers, std::mutex& dataMutex);

#endif