CXX = g++
CXXFLAGS = -std=c++17 -Iinclude -Wall -Wextra -O0 -g
SRC = $(wildcard src/*.cpp)
OBJ_DIR = build/cpp
OUT = $(OBJ_DIR)/just-weather

all: $(OUT)

$(OUT): $(SRC)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@

run: $(OUT)
	./$(OUT)

asan: CXXFLAGS += -fsanitize=address -g -O1
asan: clean $(OUT)

clean:
	rm -rf build/cpp

.PHONY: all run clean asan
