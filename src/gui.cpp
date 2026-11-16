#include "mcprobe/gui.hpp"
#include <windows.h>
#include <shlobj.h>
#include <richedit.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <atomic>
#include <mutex>
#include <filesystem>

#define ID_BTN_START   101
#define ID_BTN_STOP    102
#define ID_BTN_DUMP    103
#define ID_BTN_BROWSE  104
#define ID_EDIT_IPS    105
#define ID_EDIT_PLY    106
#define ID_EDIT_LOG    107
#define ID_EDIT_LEGEND 108
#define ID_EDIT_PATH   109

#define WM_APPEND_LOG (WM_USER + 100)

static HWND hLblIps, hLblPly, hLblLog, hEditLegend, hLblPath, hEditPath, hBtnBrowse;
static HWND hEditIps, hEditPly;
HWND hEditLog = nullptr;
static HWND hBtnStart, hBtnStop, hDump;
static HWND g_hMainWnd = nullptr;
static std::atomic<bool>* g_running = nullptr;
static std::atomic<bool>* g_startScan = nullptr;
static std::string* g_ipsRef = nullptr;
static std::string* g_plyRef = nullptr;
static std::mutex* g_dataMutex = nullptr;

static std::filesystem::path getConfigFilePath(const std::string& filename) {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::filesystem::path dir = std::filesystem::path(path) / "MinecraftStatusProbe";
        std::filesystem::create_directories(dir);
        return dir / filename;
    }
    return filename;
}

static std::string loadFileContent(const std::string& filename, const std::string& fallback) {
    auto path = getConfigFilePath(filename);
    std::ifstream file(path);
    if (!file.is_open()) return fallback;
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    std::string crlfContent;
    for (size_t i = 0; i < content.length(); ++i) {
        if (content[i] == '\n' && (i == 0 || content[i - 1] != '\r')) {
            crlfContent += "\r\n";
        } else {
            crlfContent += content[i];
        }
    }
    return crlfContent;
}

static void saveExportPathConfig(const std::wstring& pathStr) {
    auto path = getConfigFilePath("export_path.txt");
    std::ofstream file(path);
    if (file.is_open()) {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, pathStr.c_str(), -1, NULL, 0, NULL, NULL);
        std::string str(size_needed > 0 ? size_needed - 1 : 0, 0);
        if (size_needed > 1) WideCharToMultiByte(CP_UTF8, 0, pathStr.c_str(), -1, &str[0], size_needed, NULL, NULL);
        file << str;
    }
}

static std::wstring loadExportPathConfig() {
    auto path = getConfigFilePath("export_path.txt");
    if (std::filesystem::exists(path)) {
        std::ifstream file(path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            if (!content.empty()) {
                int wide_len = MultiByteToWideChar(CP_UTF8, 0, content.c_str(), -1, NULL, 0);
                std::wstring wStr(wide_len > 0 ? wide_len - 1 : 0, 0);
                if (wide_len > 1) MultiByteToWideChar(CP_UTF8, 0, content.c_str(), -1, &wStr[0], wide_len);
                return wStr;
            }
        }
    }

    char docsPathA[MAX_PATH];
    wchar_t docsPathW[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, docsPathA))) {
        MultiByteToWideChar(CP_UTF8, 0, docsPathA, -1, docsPathW, MAX_PATH);
        return std::wstring(docsPathW);
    }
    return L"C:\\";
}

void AppendTextToLog(HWND hEdit, const std::string& text) {
    if (!g_hMainWnd) return;
    std::string* pText = new std::string(text);
    PostMessageW(g_hMainWnd, WM_APPEND_LOG, 0, (LPARAM)pText);
}

static void InitializeLegendColors(HWND hLegend) {
    if (!hLegend) return;

    SetWindowTextW(hLegend, L"");

    struct LegendItem {
        std::wstring text;
        COLORREF color;
    };

    std::vector<LegendItem> items = {
        { L"● Match & Whitelist ", RGB(255, 24, 140) }, // Pink
        { L"● Match ", RGB(31, 246, 246) },             // Cyan/Teal
        { L"● Whitelist ", RGB(255, 140, 0) },          // Orange
        { L"● Online ", RGB(0, 160, 0) },               // Green
        { L"● Unreachable ", RGB(0, 100, 255) },        // Blue
        { L"● Offline ", RGB(220, 40, 40) },            // Red
        { L"● No Server ", RGB(139, 69, 19) }           // Brown
    };

    for (const auto& item : items) {
        int len = GetWindowTextLengthW(hLegend);
        SendMessageW(hLegend, EM_SETSEL, (WPARAM)len, (LPARAM)len);

        CHARFORMAT2W cf = {0};
        cf.cbSize = sizeof(CHARFORMAT2W);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = item.color;
        SendMessageW(hLegend, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

        SendMessageW(hLegend, EM_REPLACESEL, 0, (LPARAM)item.text.c_str());
    }
}

static void ExecuteAppendLog(HWND hEdit, const std::string& text) {
    if (!hEdit) return;

    SendMessageW(hEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);

    COLORREF textColor = RGB(50, 50, 50);
    if (text.find("Whitelisted Player Match") != std::string::npos || text.find("Match & Whitelist") != std::string::npos) {
        textColor = RGB(255, 24, 140);    // Pink for Match & Whitelist
    } else if (text.find("[+]") != std::string::npos) {
        textColor = RGB(31, 246, 246);    // Cyan/Teal for Match
    } else if (text.find("Whitelisted") != std::string::npos || text.find("Whitelist") != std::string::npos) {
        textColor = RGB(255, 140, 0);     // Orange for Whitelisted
    } else if (text.find("Online Server") != std::string::npos || text.find("Scan complete") != std::string::npos) {
        textColor = RGB(0, 160, 0);       // Green for Online
    } else if (text.find("[!]") != std::string::npos || text.find("Unreachable") != std::string::npos) {
        textColor = RGB(0, 100, 255);     // Blue for Unreachable
    } else if (text.find("[-]") != std::string::npos || text.find("Offline") != std::string::npos) {
        textColor = RGB(220, 40, 40);     // Red for Offline
    } else if (text.find("No server") != std::string::npos || text.find("no server") != std::string::npos) {
        textColor = RGB(139, 69, 19);     // Brown for No server
    } else if (text.find("[>]") != std::string::npos) {
        textColor = RGB(120, 120, 120);   // Gray for Checking
    }

    CHARFORMAT2W cf = {0};
    cf.cbSize = sizeof(CHARFORMAT2W);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = textColor;
    SendMessageW(hEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
    std::wstring wText(wide_len > 0 ? wide_len - 1 : 0, 0);
    if (wide_len > 1) {
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wText[0], wide_len);
    }

    std::wstring formatted = wText + L"\r\n";
    SendMessageW(hEdit, EM_REPLACESEL, 0, (LPARAM)formatted.c_str());

    SendMessageW(hEdit, EM_SCROLLCARET, 0, 0);
    SendMessageW(hEdit, WM_VSCROLL, SB_BOTTOM, 0);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hMainWnd = hwnd;
            LoadLibraryW(L"Msftedit.dll");

            std::string defaultIps = loadFileContent("targets.md", "");
            std::string defaultPlayers = loadFileContent("players.md", "");

            hLblIps = CreateWindowW(L"STATIC", L"Endpoints / IP List (from targets.md):", WS_VISIBLE | WS_CHILD, 20, 15, 310, 20, hwnd, NULL, NULL, NULL);
            hEditIps = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, 20, 40, 310, 130, hwnd, (HMENU)ID_EDIT_IPS, NULL, NULL);

            int countIps = MultiByteToWideChar(CP_UTF8, 0, defaultIps.c_str(), -1, NULL, 0);
            std::wstring wIps(countIps, 0);
            MultiByteToWideChar(CP_UTF8, 0, defaultIps.c_str(), -1, &wIps[0], countIps);
            SetWindowTextW(hEditIps, wIps.c_str());

            hLblPly = CreateWindowW(L"STATIC", L"Target Players (from players.md):", WS_VISIBLE | WS_CHILD, 350, 15, 310, 20, hwnd, NULL, NULL, NULL);
            hEditPly = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, 350, 40, 310, 130, hwnd, (HMENU)ID_EDIT_PLY, NULL, NULL);

            int countPly = MultiByteToWideChar(CP_UTF8, 0, defaultPlayers.c_str(), -1, NULL, 0);
            std::wstring wPly(countPly, 0);
            MultiByteToWideChar(CP_UTF8, 0, defaultPlayers.c_str(), -1, &wPly[0], countPly);
            SetWindowTextW(hEditPly, wPly.c_str());

            hBtnStart = CreateWindowW(L"BUTTON", L"START SCAN", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 20, 180, 110, 30, hwnd, (HMENU)ID_BTN_START, NULL, NULL);
            hBtnStop  = CreateWindowW(L"BUTTON", L"STOP SCAN", WS_VISIBLE | WS_CHILD, 135, 180, 110, 30, hwnd, (HMENU)ID_BTN_STOP, NULL, NULL);

            hEditLegend = CreateWindowW(L"RICHEDIT50W", L"", WS_VISIBLE | WS_CHILD | ES_READONLY | ES_CENTER, 250, 183, 560, 25, hwnd, (HMENU)ID_EDIT_LEGEND, NULL, NULL);
            SendMessageW(hEditLegend, EM_SETBKGNDCOLOR, 0, RGB(240, 240, 240));
            InitializeLegendColors(hEditLegend);

            hDump     = CreateWindowW(L"BUTTON", L"Dump to matches.txt", WS_VISIBLE | WS_CHILD, 820, 178, 175, 28, hwnd, (HMENU)ID_BTN_DUMP, NULL, NULL);

            hLblPath  = CreateWindowW(L"STATIC", L"Export:", WS_VISIBLE | WS_CHILD, 530, 215, 55, 20, hwnd, NULL, NULL, NULL);
            hEditPath = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 590, 212, 320, 24, hwnd, (HMENU)ID_EDIT_PATH, NULL, NULL);
            hBtnBrowse = CreateWindowW(L"BUTTON", L"Browse...", WS_VISIBLE | WS_CHILD, 918, 212, 72, 24, hwnd, (HMENU)ID_BTN_BROWSE, NULL, NULL);

            std::wstring savedPath = loadExportPathConfig();
            SetWindowTextW(hEditPath, savedPath.c_str());

            hLblLog = CreateWindowW(L"STATIC", L"Live Output & Results:", WS_VISIBLE | WS_CHILD, 20, 245, 250, 20, hwnd, NULL, NULL, NULL);
            hEditLog = CreateWindowW(L"RICHEDIT50W", L"[*] Ready to scan...\r\n", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY, 20, 270, 970, 370, hwnd, (HMENU)ID_EDIT_LOG, NULL, NULL);
            break;
        }
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            if (width < 500 || height < 300) break;

            int halfWidth = (width - 60) / 2;
            int topEditHeight = 130;

            MoveWindow(hLblIps, 20, 15, halfWidth, 20, TRUE);
            MoveWindow(hEditIps, 20, 40, halfWidth, topEditHeight, TRUE);

            int rightX = 30 + halfWidth;
            MoveWindow(hLblPly, rightX, 15, halfWidth, 20, TRUE);
            MoveWindow(hEditPly, rightX, 40, halfWidth, topEditHeight, TRUE);

            MoveWindow(hBtnStart, 20, 180, 110, 30, TRUE);
            MoveWindow(hBtnStop, 135, 180, 110, 30, TRUE);

            MoveWindow(hDump, width - 195, 178, 175, 28, TRUE);
            MoveWindow(hLblPath, width - 480, 215, 55, 20, TRUE);
            MoveWindow(hEditPath, width - 420, 212, 335, 24, TRUE);
            MoveWindow(hBtnBrowse, width - 80, 212, 72, 24, TRUE);

            int legendWidth = width - 440;
            if (legendWidth < 200) legendWidth = 200;
            MoveWindow(hEditLegend, 250, 183, legendWidth, 25, TRUE);

            MoveWindow(hLblLog, 20, 245, 250, 20, TRUE);
            int logHeight = height - 285;
            if (logHeight < 50) logHeight = 50;
            MoveWindow(hEditLog, 20, 270, width - 40, logHeight, TRUE);
            break;
        }
        case WM_APPEND_LOG: {
            std::string* pText = (std::string*)lParam;
            if (pText) {
                ExecuteAppendLog(hEditLog, *pText);
                delete pText;
            }
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == ID_BTN_START) {
                int lenIps = GetWindowTextLengthW(hEditIps);
                std::wstring wIps(lenIps + 1, L'\0');
                if (lenIps > 0) GetWindowTextW(hEditIps, &wIps[0], lenIps + 1);
                int size_needed_ips = WideCharToMultiByte(CP_UTF8, 0, wIps.c_str(), -1, NULL, 0, NULL, NULL);
                std::string strIps(size_needed_ips > 0 ? size_needed_ips - 1 : 0, 0);
                if (size_needed_ips > 1) WideCharToMultiByte(CP_UTF8, 0, wIps.c_str(), -1, &strIps[0], size_needed_ips, NULL, NULL);

                int lenPly = GetWindowTextLengthW(hEditPly);
                std::wstring wPly(lenPly + 1, L'\0');
                if (lenPly > 0) GetWindowTextW(hEditPly, &wPly[0], lenPly + 1);
                int size_needed_ply = WideCharToMultiByte(CP_UTF8, 0, wPly.c_str(), -1, NULL, 0, NULL, NULL);
                std::string strPly(size_needed_ply > 0 ? size_needed_ply - 1 : 0, 0);
                if (size_needed_ply > 1) WideCharToMultiByte(CP_UTF8, 0, wPly.c_str(), -1, &strPly[0], size_needed_ply, NULL, NULL);

                {
                    std::lock_guard<std::mutex> lock(*g_dataMutex);
                    if (g_ipsRef) *g_ipsRef = strIps;
                    if (g_plyRef) *g_plyRef = strPly;
                }
                if (g_startScan) g_startScan->store(true);

                AppendTextToLog(hEditLog, "[*] Scan started...");

            } else if (id == ID_BTN_STOP) {
                if (g_running) *g_running = false;
                AppendTextToLog(hEditLog, "[*] Stop signal sent...");
            } else if (id == ID_BTN_BROWSE) {
                WCHAR szDir[MAX_PATH];
                BROWSEINFOW bi = {0};
                bi.hwndOwner = hwnd;
                bi.lpszTitle = L"Select Custom Export Directory";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

                LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
                if (pidl != NULL) {
                    if (SHGetPathFromIDListW(pidl, szDir)) {
                        SetWindowTextW(hEditPath, szDir);
                        saveExportPathConfig(szDir);
                    }
                    CoTaskMemFree(pidl);
                }
            } else if (id == ID_BTN_DUMP) {
                int lenPath = GetWindowTextLengthW(hEditPath);
                std::wstring dirW(lenPath + 1, L'\0');
                if (lenPath > 0) GetWindowTextW(hEditPath, &dirW[0], lenPath + 1);
                dirW.resize(lenPath);

                if (dirW.empty()) {
                    MessageBoxW(hwnd, L"Please specify a valid export directory path.", L"Error", MB_OK | MB_ICONERROR);
                    break;
                }

                saveExportPathConfig(dirW);

                int lenLog = GetWindowTextLengthW(hEditLog);
                std::vector<wchar_t> bufLog(lenLog + 1);
                GetWindowTextW(hEditLog, bufLog.data(), lenLog + 1);

                int size_needed = WideCharToMultiByte(CP_UTF8, 0, bufLog.data(), lenLog, NULL, 0, NULL, NULL);
                std::string logStr(size_needed, 0);
                WideCharToMultiByte(CP_UTF8, 0, bufLog.data(), lenLog, &logStr[0], size_needed, NULL, NULL);

                std::filesystem::path targetDir(dirW);
                std::filesystem::create_directories(targetDir);
                std::filesystem::path dumpPath = targetDir / "matches.txt";

                std::ofstream dump(dumpPath, std::ios::app);
                if (!dump.is_open()) {
                    MessageBoxW(hwnd, L"Failed to create matches.txt in the specified directory. Check permissions!", L"Error", MB_OK | MB_ICONERROR);
                    break;
                }

                std::stringstream stream(logStr);
                std::string line;
                int matchCount = 0;

                while (std::getline(stream, line)) {
                    if (!line.empty() && line.back() == '\r') {
                        line.pop_back();
                    }

                    std::string targetEntry = "";
                    if (line.find("[+] Whitelisted Player Match Found:") != std::string::npos || line.find("[+] Player Match Found:") != std::string::npos) {
                        size_t pos = line.find(": ");
                        if (pos != std::string::npos) {
                            targetEntry = line.substr(pos + 2);
                        }
                    } else if (line.find("Whitelisted") != std::string::npos || line.find("Whitelist") != std::string::npos) {
                        size_t pos = line.find(": ");
                        if (pos != std::string::npos) {
                            targetEntry = line.substr(pos + 2) + " (Whitelisted)";
                        }
                    } else if (line.find("[*] Online Server (No Target Match):") != std::string::npos) {
                        size_t pos = line.find(": ");
                        if (pos != std::string::npos) {
                            targetEntry = line.substr(pos + 2) + " (Online - No Target Match)";
                        }
                    }

                    if (!targetEntry.empty()) {
                        dump << targetEntry << "\n";
                        matchCount++;
                    }
                }
                dump.close();

                if (matchCount > 0) {
                    std::wstring successMsg = L"Successfully appended matches to:\n" + dumpPath.wstring();
                    MessageBoxW(hwnd, successMsg.c_str(), L"Export Complete", MB_OK | MB_ICONINFORMATION);
                } else {
                    MessageBoxW(hwnd, L"No online servers found in the log to dump yet.", L"Notice", MB_OK | MB_ICONWARNING);
                }
            }
            break;
        }
        case WM_DESTROY:
            if (g_running) *g_running = false;
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void launchGuiApp(std::atomic<bool>& isRunning, std::atomic<bool>& startScan, std::string& outIps, std::string& outPlayers, std::mutex& dataMutex) {
    g_running = &isRunning;
    g_startScan = &startScan;
    g_ipsRef = &outIps;
    g_plyRef = &outPlayers;
    g_dataMutex = &dataMutex;

    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"MinecraftProbeClass";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        0, L"MinecraftProbeClass", L"Minecraft Server & Player Probe",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1020, 700,
        NULL, NULL, hInstance, NULL
    );

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}