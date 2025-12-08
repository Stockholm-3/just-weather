CXX = g++
CXXFLAGS = -std=c++17 -Iinclude -Wall -Wextra -O0 -g
SRC = $(wildcard src/*.cpp)
OUT = ../build/cpp/just-weather

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

.PHONY: all run clean asan
