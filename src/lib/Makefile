# --- toolchain ---
CC=gcc
CFLAGS_BASE=-Wall -Wno-psabi -Wfatal-errors -Werror
DEBUG_FLAGS=-g -O0
INCLUDES=-I. -Iunitests
LIBS=-luuid -lcurl -pthread -lm -lbsd

# --- mode (по умолчанию tests) ---
MODE ?= tests

# --- sources per mode ---
ifeq ($(MODE),tests)
  CFLAGS=$(CFLAGS_BASE) $(DEBUG_FLAGS) -DUNIT_TESTS $(INCLUDES)
  SOURCES= \
    main.c \
    unitests/parse_tests.c parse.c \
    unitests/tcp_client_tests.c tcp_client.c
else ifeq ($(MODE),debug)
  CFLAGS=$(CFLAGS_BASE) $(DEBUG_FLAGS) $(INCLUDES)
  # Debug build without unit tests
  SOURCES=main.c parse.c tcp_client.c
else
  CFLAGS=$(CFLAGS_BASE) -DNDEBUG $(INCLUDES)
  SOURCES=main.c parse.c tcp_client.c
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

run: $(EXECUTABLE)
	@echo "Running $(EXECUTABLE)..."
	@./$(EXECUTABLE)

clean:
	@echo "Cleaning..."
	@rm -rf $(BUILD_DIR) $(EXECUTABLE)

.PHONY: all clean run
