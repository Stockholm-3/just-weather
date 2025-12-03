# ------------------------------------------------------------
# Compiler + global settings
# ------------------------------------------------------------
CC          := gcc
SRC_DIR     := src
# Separate build folders per mode
BUILD_MODE  ?= debug
BUILD_DIR   := build/$(BUILD_MODE)
BIN_SERVER  := $(BUILD_DIR)/server/just-weather
BIN_CLIENT  := $(BUILD_DIR)/client/just-weather
BIN_FUZZ    := $(BUILD_DIR)/fuzz/fuzz_http

# ------------------------------------------------------------
# Build configuration
# ------------------------------------------------------------
ifeq ($(BUILD_MODE),release)
	CFLAGS_BASE := -O3 -DNDEBUG
	BUILD_TYPE  := Release
else
	CFLAGS_BASE := -O1 -g
	BUILD_TYPE  := Debug
endif


# ------------------------------------------------------------
# Compiler and linker flags
# ------------------------------------------------------------
CFLAGS      := $(CFLAGS_BASE) -Wall -Werror -Wfatal-errors -MMD -MP \
               -Ilib/jansson -Isrc/lib -Isrc/server -Iincludes

JANSSON_CFLAGS := $(filter-out -Werror -Wfatal-errors,$(CFLAGS)) -w

LDFLAGS     := -flto -Wl,--gc-sections
LIBS        := -lcurl #curl wont bes used anymore!!

# Fuzz-specific flags
FUZZ_CFLAGS := -Wall -Wextra -g -O1 -fsanitize=address,undefined
FUZZ_LDFLAGS := -fsanitize=address,undefined

# ------------------------------------------------------------
# Source and object files
# ------------------------------------------------------------
SRC_SERVER := $(shell find -L $(SRC_DIR)/server -type f -name '*.c' ! -path "*/jansson/*") \
              $(shell find -L $(SRC_DIR)/lib -type f -name '*.c' ! -path "*/jansson/*")

SRC_CLIENT := $(shell find -L $(SRC_DIR)/client -type f -name '*.c' ! -path "*/jansson/*") \
              $(shell find -L $(SRC_DIR)/lib -type f -name '*.c' ! -path "*/jansson/*")

SRC_FUZZ := tests/fuzz_http.c

OBJ_SERVER  := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_SERVER))
OBJ_CLIENT  := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_CLIENT))
OBJ_FUZZ    := $(BUILD_DIR)/tests/fuzz_http.o

DEP_SERVER  := $(OBJ_SERVER:.o=.d)
DEP_CLIENT  := $(OBJ_CLIENT:.o=.d)
DEP_FUZZ    := $(OBJ_FUZZ:.o=.d)

# ------------------------------------------------------------
# Jansson integration
# ------------------------------------------------------------
JANSSON_SRC := $(shell find lib/jansson/ -maxdepth 1 -type f -name '*.c')
JANSSON_OBJ := $(patsubst lib/jansson/%.c,$(BUILD_DIR)/jansson/%.o,$(JANSSON_SRC))
OBJ_SERVER  += $(JANSSON_OBJ)
OBJ_CLIENT  += $(JANSSON_OBJ)

# ------------------------------------------------------------
# Build rules
# ------------------------------------------------------------
.PHONY: all
all: $(BIN_SERVER) $(BIN_CLIENT)
	@echo "Build complete. [$(BUILD_TYPE)]"

$(BIN_SERVER): $(OBJ_SERVER)
	@$(CC) $(LDFLAGS) $(OBJ_SERVER) -o $@ $(LIBS)

$(BIN_CLIENT): $(OBJ_CLIENT)
	@$(CC) $(LDFLAGS) $(OBJ_CLIENT) -o $@ $(LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<... [$(BUILD_TYPE)]"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/jansson/%.o: lib/jansson/%.c
	@echo "Compiling Jansson $<... [$(BUILD_TYPE)]"
	@mkdir -p $(dir $@)
	@$(CC) $(JANSSON_CFLAGS) -c $< -o $@

# ------------------------------------------------------------
# Fuzz rules
# ------------------------------------------------------------
$(BIN_FUZZ): $(OBJ_FUZZ)
	@echo "Linking fuzz with sanitizers..."
	@mkdir -p $(dir $@)
	@$(CC) $(FUZZ_LDFLAGS) $(OBJ_FUZZ) -o $@
	@echo "Fuzz built: $(BIN_FUZZ)"

$(OBJ_FUZZ): tests/fuzz_http.c
	@echo "Compiling fuzz $<..."
	@mkdir -p $(dir $@)
	@$(CC) $(FUZZ_CFLAGS) -MMD -MP -c $< -o $@

.PHONY: fuzz
fuzz: $(BIN_FUZZ)
	@echo "Fuzz ready for testing."

# Create sample fuzz inputs
.PHONY: fuzz-inputs
fuzz-inputs:
	@mkdir -p tests/inputs
	@echo -e "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n" > tests/inputs/get.txt
	@echo -e "POST /api HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello" > tests/inputs/post.txt
	@echo -e "GET /../etc/passwd HTTP/1.1\r\nHost: evil.com\r\n\r\n" > tests/inputs/traversal.txt
	@echo -e "GET / HTTP/1.1\r\nHost: test\nInjected: header\r\n\r\n" > tests/inputs/injection.txt
	@echo -e "POST / HTTP/1.1\r\nContent-Length: 99999\r\n\r\nshort" > tests/inputs/length_mismatch.txt
	@echo "Created sample fuzz inputs in tests/inputs/"

# Run fuzz tests
.PHONY: fuzz-test
fuzz-test: $(BIN_FUZZ) fuzz-inputs
	@echo "Running fuzz tests..."
	@set -e; for file in tests/inputs/*.txt; do \
		echo "=== Testing $$file ==="; \
		$(BIN_FUZZ) "$$file" || exit 1; \
	done
	@echo "Fuzz tests complete."

# ------------------------------------------------------------
# Release target
# ------------------------------------------------------------
.PHONY: release
release:
	@$(MAKE) --no-print-directory BUILD_MODE=release all

# ------------------------------------------------------------
# Debugging and Sanitizers
# ------------------------------------------------------------
.PHONY: debug
debug:
	@$(MAKE) --no-print-directory BUILD_MODE=debug all

# Run server under GDB
.PHONY: gdb-server
gdb-server: $(BIN_SERVER)
	@echo "Launching server in GDB..."
	@gdb --quiet --args ./$(BIN_SERVER)

# Run client under GDB
.PHONY: gdb-client
gdb-client: $(BIN_CLIENT)
	@echo "Launching client in GDB..."
	@gdb --quiet --args ./$(BIN_CLIENT)

# AddressSanitizer build (for memory debugging)
.PHONY: asan
asan:
	@echo "Building with AddressSanitizer..."
	@$(MAKE) --no-print-directory \
		CFLAGS_BASE="-g -O1 -fsanitize=address -fno-omit-frame-pointer" \
		LDFLAGS="-fsanitize=address" \
		all

# ------------------------------------------------------------
# Utilities
# ------------------------------------------------------------
.PHONY: run-server
run-server: $(BIN_SERVER)
	./$(BIN_SERVER)

.PHONY: run-client
run-client: $(BIN_CLIENT)
	./$(BIN_CLIENT)

.PHONY: clean
clean:
	@rm -rf build
	@rm -rf tests/inputs
	@echo "Cleaned build artifacts."


# ------------------------------------------------------------
# Start server in detached tmux session
# ------------------------------------------------------------
.PHONY: start-server
start-server: $(BIN_SERVER)
	@SESSION_NAME=just-weather-server; \
	if tmux has-session -t $$SESSION_NAME 2>/dev/null; then \
		echo "Session '$$SESSION_NAME' already exists. Attaching..."; \
		tmux attach -t $$SESSION_NAME; \
	else \
		echo "Starting server in detached tmux session '$$SESSION_NAME'..."; \
		tmux new -d -s $$SESSION_NAME './$(BIN_SERVER)'; \
		echo "Server started in tmux session '$$SESSION_NAME'."; \
	fi

# Show formatting errors without modifying files
.PHONY: format
format:
	@echo "Checking formatting..."
	@unformatted=$$(find . \( -name '*.c' -o -name '*.h' \) -print0 | \
		xargs -0 clang-format -style=file -output-replacements-xml | \
		grep -c "<replacement " || true); \
	if [ $$unformatted -ne 0 ]; then \
		echo "$$unformatted file(s) need formatting"; \
		find . \( -name '*.c' -o -name '*.h' \) -print0 | \
		xargs -0 clang-format -style=file -n -Werror; \
		exit 1; \
	else \
		echo "All files properly formatted"; \
	fi

# Actually fixes formatting
.PHONY: format-fix
format-fix:
	@echo "Applying clang-format..."
	find . \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 clang-format -i -style=file
	@echo "Formatting applied."

.PHONY: lint
lint:
	@echo "Running clang-tidy using compile_commands.json..."
	@find src \( -name '*.c' -o -name '*.h' \) ! -path "*/jansson/*" -print0 | \
	while IFS= read -r -d '' file; do \
		echo "→ Linting $$file"; \
		clang-tidy "$$file" \
			--config-file=.clang-tidy \
			--quiet \
			--header-filter='^(src|includes|lib)/' \
			--system-headers=false || true; \
	done
	@echo "Lint complete (see warnings above)."

# ------------------------------------------------------------
# GitHub Actions (act)
# ------------------------------------------------------------
.PHONY: workflow-build
workflow-build:
	DOCKER_HOST="$${DOCKER_HOST}" act push --job build \
       -P ubuntu-latest=catthehacker/ubuntu:act-latest

.PHONY: workflow-format
workflow-format:
	DOCKER_HOST="$${DOCKER_HOST}" act push --job format-check \
       -P ubuntu-latest=teeks99/clang-ubuntu:19

.PHONY: workflow
workflow: workflow-build workflow-format

# ------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------
-include $(DEP_SERVER)
-include $(DEP_CLIENT)
-include $(DEP_FUZZ)