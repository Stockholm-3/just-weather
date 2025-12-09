CXX = g++
CXXFLAGS = -std=c++17 -Iinclude -Wall -Wextra -O0 -g
SRC = $(wildcard src/*.cpp)
OUT = ../build/cpp/just-weather

# Try to detect if nlohmann/json is installed system-wide
# If not, assume json.hpp is in the include directory
ifeq ($(shell pkg-config --exists nlohmann_json 2>/dev/null && echo yes),yes)
    CXXFLAGS += $(shell pkg-config --cflags nlohmann_json)
    $(info Using system nlohmann/json via pkg-config)
else
    # Fallback: assume json.hpp is in include/ directory
    $(info Using nlohmann/json from include/ directory)
endif

all: $(OUT)

$(OUT): $(SRC)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@

run: $(OUT)
	../build/cpp/just-weather

asan: CXXFLAGS += -fsanitize=address -g -O1
asan: clean $(OUT)

clean:
	rm -f ../build/cpp/just-weather

# Helper target to download json.hpp if needed
download-json:
	@if [ ! -f include/nlohmann/json.hpp ]; then \
		echo "Downloading nlohmann/json.hpp..."; \
		mkdir -p include/nlohmann; \
		curl -L https://github.com/nlohmann/json/releases/latest/download/json.hpp \
			-o include/nlohmann/json.hpp; \
		echo "Downloaded to include/nlohmann/json.hpp"; \
	else \
		echo "json.hpp already exists in include/nlohmann/"; \
	fi

.PHONY: all run clean asan download-json