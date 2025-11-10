# --- toolchain ---
CC=gcc
CFLAGS_BASE=-Wall -Wno-psabi -Wfatal-errors -Werror
DEBUG_FLAGS=-g -O0
INCLUDES=-I. -Itests
LIBS=-luuid -lcurl -pthread -lm -lbsd

# --- mode (по умолчанию tests) ---
MODE ?= tests

# --- sources per mode ---
ifeq ($(MODE),tests)
  CFLAGS=$(CFLAGS_BASE) $(DEBUG_FLAGS) -DUNIT_TESTS $(INCLUDES)
  SOURCES= \
    main.c \
    tests/parse_tests.c parse.c \
    tests/tcp_client_tests.c TCPClient.c
else ifeq ($(MODE),debug)
  CFLAGS=$(CFLAGS_BASE) $(DEBUG_FLAGS) $(INCLUDES)
  # Укажи реальные файлы твоего приложения (без тестов и без второго main):
  SOURCES=main.c parse.c TCPClient.c
else
  CFLAGS=$(CFLAGS_BASE) -DNDEBUG $(INCLUDES)
  SOURCES=main.c parse.c TCPClient.c
endif

# --- build rules ---
BUILD_DIR=build
OBJECTS=$(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))
EXECUTABLE=main

all: $(EXECUTABLE)
	@echo "Build complete ($(MODE))."

$(EXECUTABLE): $(OBJECTS)
	@echo "Linking $(EXECUTABLE)..."
	@$(CC) $(OBJECTS) -o $@ $(LIBS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "Cleaning..."
	@rm -rf $(BUILD_DIR) $(EXECUTABLE)

.PHONY: all clean
