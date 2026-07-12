#pragma once
#include <windows.h>
#include <winternl.h>
#include <intrin.h>
#include <cstdint>
#include <cstring>
#include <cstdlib>

// -------------------------------------------------------------------
// XOR key for compile-time / runtime encryption
// -------------------------------------------------------------------
#define XOR_KEY 0xAB

// -------------------------------------------------------------------
// XOR string obfuscation macro - narrow (char) version
// Usage:  XOR_LIT("hello world")
// Decrypts at runtime from static encrypted storage, returns const char*
// -------------------------------------------------------------------
#define XOR_LIT(str)                                                       \
    []() -> const char* {                                                  \
        static char _buf[sizeof(str)];                                     \
        static bool _init = false;                                         \
        if (!_init) {                                                      \
            for (size_t _i = 0; _i < sizeof(str); ++_i)                    \
                _buf[_i] = static_cast<char>(str[_i] ^ XOR_KEY);           \
            _buf[sizeof(str) - 1] = '\0';                                  \
            _init = true;                                                  \
        }                                                                  \
        return const_cast<const char*>(_buf);                              \
    }()

// -------------------------------------------------------------------
// XOR string obfuscation macro - wide (wchar_t) version
// Usage:  XOR_LIT_W(L"hello world")
// Returns const wchar_t*
// -------------------------------------------------------------------
#define XOR_LIT_W(wstr)                                                    \
    []() -> const wchar_t* {                                               \
        static wchar_t _buf[sizeof(wstr) / sizeof(wchar_t)];               \
        static bool _init = false;                                         \
        if (!_init) {                                                      \
            for (size_t _i = 0; _i < (sizeof(wstr) / sizeof(wchar_t)) - 1; ++_i) \
                _buf[_i] = static_cast<wchar_t>(wstr[_i] ^ XOR_KEY);       \
            _buf[(sizeof(wstr) / sizeof(wchar_t)) - 1] = L'\0';            \
            _init = true;                                                  \
        }                                                                  \
        return const_cast<const wchar_t*>(_buf);                           \
    }()

// -------------------------------------------------------------------
// Runtime XOR decrypt/encrypt (symmetric)
// -------------------------------------------------------------------
inline void xor_buffer(char* buf, size_t len, uint8_t key = XOR_KEY) {
    for (size_t i = 0; i < len; ++i)
        buf[i] ^= static_cast<char>(key);
}

// -------------------------------------------------------------------
// Runtime XOR decrypt a string into a pre-allocated buffer
// -------------------------------------------------------------------
inline void xor_decrypt_string(char* out, const unsigned char* in, size_t len, uint8_t key = XOR_KEY) {
    for (size_t i = 0; i < len; ++i)
        out[i] = static_cast<char>(in[i] ^ key);
    out[len] = '\0';
}

// -------------------------------------------------------------------
// Allocate and decrypt a string (caller must LocalFree)
// -------------------------------------------------------------------
inline char* xor_decrypt_alloc(const unsigned char* in, size_t len, uint8_t key = XOR_KEY) {
    char* out = static_cast<char*>(LocalAlloc(LMEM_FIXED, len + 1));
    if (!out) return nullptr;
    xor_decrypt_string(out, in, len, key);
    return out;
}

// -------------------------------------------------------------------
// Anti-debug: IsDebuggerPresent
// -------------------------------------------------------------------
inline bool antidebug_IsDebuggerPresent() {
    return IsDebuggerPresent() != FALSE;
}

// -------------------------------------------------------------------
// Anti-debug: NtGlobalFlag (PEB BeingDebugged flag)
// -------------------------------------------------------------------
inline bool antidebug_NtGlobalFlag() {
    PPEB peb = reinterpret_cast<PPEB>(__readgsqword(0x60));
    if (!peb) return false;
    return peb->BeingDebugged != 0;
}

// -------------------------------------------------------------------
// Anti-debug: RDTSC timing check
// -------------------------------------------------------------------
inline bool antidebug_RDTSC() {
    uint64_t t1 = __rdtsc();
    for (volatile int i = 0; i < 100; ++i) {}
    uint64_t t2 = __rdtsc();
    return (t2 - t1) > 0xFFFF;
}

// -------------------------------------------------------------------
// Run all anti-debug checks; return true if debugger suspected
// -------------------------------------------------------------------
inline bool antidebug_CheckAll() {
    if (antidebug_IsDebuggerPresent())     return true;
    if (antidebug_NtGlobalFlag())         return true;
    if (antidebug_RDTSC())                return true;
    return false;
}
