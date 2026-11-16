MinecraftStatusProbe
====================
Made By Skittlegodhalo

MinecraftStatusProbe is a high-performance, multi-threaded Win32 GUI application written in C++ designed to scan Minecraft server endpoints, query their Server List Ping (SLP) status, search for specific target players online, and log results in real-time with color-coded feedback.

--------------------------------------------------------------------------------
IMPORTANT NETWORK & ISP WARNING
--------------------------------------------------------------------------------

Network Usage & ISP Flag Warning: When running this tool on a standard home internet connection, it is strongly recommended to keep concurrent endpoint scans capped in reasonable batches (50 to 100 endpoints at a time).

- ISP Security Filters: Internet Service Providers monitor automated outbound traffic patterns. Hammering thousands of random public IP addresses simultaneously mimics the behavior of a botnet or vulnerability scanner, which can trigger automated abuse flags and result in temporary ISP throttling or connection suspension.

- Consumer Router Limits: Home routers maintain a connection state tracking table (NAT). Flooding your local router with hundreds of high-frequency TCP connection attempts concurrently can overwhelm its memory table, causing packet drops or temporary loss of your local internet connection.

Always break large scanning lists into manageable chunks or maintain moderate pacing to ensure stable operation.

--------------------------------------------------------------------------------
KEY FEATURES
--------------------------------------------------------------------------------

- Responsive Win32 GUI: Features a modern, resizable interface initialized at a default size of 1020x700 pixels with robust layout handling.
- Multi-Threaded Scanning Engine: Utilizes asynchronous background worker pools to check dozens of endpoints concurrently without blocking or freezing the interface.
- Advanced Whitelist & Gatekeeper Detection: Performs automated secondary login handshakes (Protocol 767) to accurately identify and flag active whitelisted or restricted servers—even when public status pings are blocked.
- Color-Coded Status Logs: Built on top of RICHEDIT50W controls with per-character formatting to give you instant visual distinction.
- Data Persistence & Export: Loads initial endpoints and target player lists from external files, saves user preferences under %APPDATA%, and allows one-click exporting of successful matches and whitelisted servers to matches.txt.

--------------------------------------------------------------------------------
COLOR CODE LEGEND
--------------------------------------------------------------------------------

- Pink (RGB 255, 24, 140) - Match & Whitelist: Target player match found on an active whitelisted server.
- Cyan/Teal (RGB 31, 246, 246) - Match: Target player match found on a public server.
- Orange - Whitelist: Server is online and protected behind an active whitelist or restriction gate.
- Green - Online: Server is public and online.
- Blue - Unreachable: Server is unreachable (network timeout / unreachable address).
- Red - Offline: Server is offline / refused connection.
- Brown - No Server: No server running on the target port.

--------------------------------------------------------------------------------
CONFIGURATION FILES
--------------------------------------------------------------------------------

Sample configuration files are included in the repository root:

- targets.md: Explicit development endpoints, one IP:port per line. The included entries use loopback addresses only; add only endpoints you own or are authorized to test.
- players.md: Optional player names from your local test server, one per line. Lines beginning with # are comments.

At runtime, the application reads and saves these files under %APPDATA%\MinecraftStatusProbe. Copy the sample files there to use them as the initial configuration. The application does not load targets.txt or players.txt.