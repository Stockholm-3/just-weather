SHELL := bash
# ============================================================================
# Makefile for Just Weather Client
# ============================================================================
# Paths relative to this Makefile location (src/client/)
CC          := gcc
CLIENT_DIR  := .
JANSSON_DIR := ../../lib/jansson
INCLUDES    := ../../includes
PROJECT_ROOT := ../..

# Build configuration
BUILD_MODE  ?= debug
BUILD_DIR   := $(PROJECT_ROOT)/build/$(BUILD_MODE)
BIN_CLIENT  := $(BUILD_DIR)/client/just-weather

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

# ============================================================================
# Compiler flags
# ============================================================================
CFLAGS := $(CFLAGS_BASE) -Wall -Werror -Wfatal-errors -MMD -MP \
          -I$(JANSSON_DIR) -I$(CLIENT_DIR) \
          -I$(CLIENT_DIR)/api -I$(CLIENT_DIR)/network -I$(CLIENT_DIR)/utils \
          -I$(INCLUDES) \
          -DCLIENT_VERSION=\"1.0.0\"

# Jansson uses format strings that may trigger truncation warnings
# We compile Jansson with same flags but without error promotion
JANSSON_CFLAGS := $(CFLAGS_BASE) -Wall -MMD -MP \
                  -I$(JANSSON_DIR) -I$(CLIENT_DIR) \
                  -I$(CLIENT_DIR)/api -I$(CLIENT_DIR)/network -I$(CLIENT_DIR)/utils \
                  -I$(INCLUDES) \
                  -Wno-format-truncation

LDFLAGS := -flto -Wl,--gc-sections
LIBS    :=

# ============================================================================
# Source files
# ============================================================================
# Client source files only (from src/client/ directory)
SRC_CLIENT := $(shell find -L $(CLIENT_DIR) -maxdepth 1 -type f -name '*.c') \
              $(shell find -L $(CLIENT_DIR)/api -type f -name '*.c') \
              $(shell find -L $(CLIENT_DIR)/network -type f -name '*.c') \
              $(shell find -L $(CLIENT_DIR)/utils -type f -name '*.c')

# Jansson source files
JANSSON_SRC := $(JANSSON_DIR)/dump.c \
               $(JANSSON_DIR)/error.c \
               $(JANSSON_DIR)/hashtable.c \
               $(JANSSON_DIR)/hashtable_seed.c \
               $(JANSSON_DIR)/load.c \
               $(JANSSON_DIR)/memory.c \
               $(JANSSON_DIR)/pack_unpack.c \
               $(JANSSON_DIR)/strbuffer.c \
               $(JANSSON_DIR)/strconv.c \
               $(JANSSON_DIR)/utf.c \
               $(JANSSON_DIR)/value.c \
               $(JANSSON_DIR)/version.c

# Object files
OBJ_CLIENT  := $(patsubst $(CLIENT_DIR)/%.c,$(BUILD_DIR)/client/%.o,$(SRC_CLIENT))
OBJ_JANSSON := $(patsubst $(JANSSON_DIR)/%.c,$(BUILD_DIR)/jansson/%.o,$(JANSSON_SRC))

# All objects needed
ALL_OBJ := $(OBJ_CLIENT) $(OBJ_JANSSON)

# Dependencies
DEP_CLIENT  := $(OBJ_CLIENT:.o=.d)
DEP_JANSSON := $(OBJ_JANSSON:.o=.d)

# ============================================================================
# Build targets
# ============================================================================
.PHONY: all
all: $(BIN_CLIENT)
	@echo "Build complete [$(BUILD_TYPE)]"

$(BIN_CLIENT): $(OBJ_CLIENT) $(OBJ_JANSSON)
	@echo "Linking client binary..."
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) $(OBJ_CLIENT) $(OBJ_JANSSON) -o $@ $(LIBS)

$(BUILD_DIR)/client/%.o: $(CLIENT_DIR)/%.c
	@echo "Compiling client $<..."
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/jansson/%.o: $(JANSSON_DIR)/%.c
	@echo "Compiling Jansson $<..."
	@mkdir -p $(dir $@)
	@$(CC) $(JANSSON_CFLAGS) -c $< -o $@

# ============================================================================
# Build modes
# ============================================================================
.PHONY: debug
debug:
	@$(MAKE) --no-print-directory BUILD_MODE=debug all

.PHONY: release
release:
	@$(MAKE) --no-print-directory BUILD_MODE=release all

# ------------------------------------------------------------
# Debugging and Sanitizers
# ------------------------------------------------------------
.PHONY: debug
debug:
# ------------------------------------------------------------
# Utilities
# ------------------------------------------------------------
.PHONY: run-client
run-client: $(BIN_CLIENT)
	@echo "Just Weather Client - Usage Examples:"
	@echo ""
	@./$(BIN_CLIENT) 2>&1 || true

.PHONY: test
test: $(BIN_CLIENT)
	@echo "Testing client with Stockholm weather..."
	@./$(BIN_CLIENT) current 59.33 18.07

.PHONY: test-client
test-client: $(BIN_CLIENT)
	@echo "Testing client with Stockholm weather..."
	@./$(BIN_CLIENT) current 59.33 18.07

.PHONY: test-city
test-city: $(BIN_CLIENT)
	@echo "Testing client with city name..."
	@./$(BIN_CLIENT) weather Stockholm SE

.PHONY: test-client-city
test-client-city: $(BIN_CLIENT)
	@echo "Testing client with city name..."
	@./$(BIN_CLIENT) weather Stockholm SE

.PHONY: test-search
test-search: $(BIN_CLIENT)
	@echo "Testing city search..."
	@./$(BIN_CLIENT) cities Stock

.PHONY: test-client-search
test-client-search: $(BIN_CLIENT)
	@echo "Testing client city search..."
	@./$(BIN_CLIENT) cities Stock

.PHONY: interactive
interactive: $(BIN_CLIENT)
	@./$(BIN_CLIENT) interactive

.PHONY: client-interactive
client-interactive: $(BIN_CLIENT)
	@./$(BIN_CLIENT) interactive

# ============================================================================
# Debugging
# ============================================================================
.PHONY: gdb
gdb: $(BIN_CLIENT)
	@echo "Launching client in GDB..."
	@gdb --quiet --args ./$(BIN_CLIENT)

.PHONY: gdb-client
gdb-client: $(BIN_CLIENT)
	@echo "Launching client in GDB..."
	@gdb --quiet --args ./$(BIN_CLIENT)

.PHONY: asan
asan:
	@echo "Building with AddressSanitizer..."
	@$(MAKE) --no-print-directory \
		CFLAGS_BASE="-g -O1 -fsanitize=address -fno-omit-frame-pointer" \
		LDFLAGS="-fsanitize=address" \
		all

.PHONY: valgrind
valgrind: $(BIN_CLIENT)
	@echo "Running under Valgrind..."
	@valgrind --leak-check=full --show-leak-kinds=all ./$(BIN_CLIENT)

# ============================================================================
# Cleanup
# ============================================================================
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
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
		echo "Linting $$file"; \
		clang-tidy "$$file" \
			--config-file=.clang-tidy \
			--quiet \
			--header-filter='^(src)/' \
			--system-headers=false || true; \
	done
	@echo "Lint complete (see warnings above)."

.PHONY: lint-fix
lint-fix:
	@echo "Running clang-tidy with auto-fix on src/ (excluding jansson)..."
	@find src \( -name '*.c' -o -name '*.h' \) ! -path "*/jansson/*" -print0 | \
	while IFS= read -r -d '' file; do \
		echo "Fixing $$file"; \
		clang-tidy "$$file" \
			--config-file=.clang-tidy \
			--fix \
			--fix-errors \
			--header-filter='src/.*\.(h|hpp)$$' \
			--system-headers=false || true; \
	done
	@echo "Auto-fix complete. Please review changes with 'git diff'."

# CI target: fails only on naming violations
.PHONY: lint-ci
lint-ci:
	@echo "Running clang-tidy for CI (naming violations = errors)..."
	@rm -f /tmp/clang-tidy-failed
	@find src \( -name '*.c' -o -name '*.h' \) ! -path "*/jansson/*" -print0 | \
	while IFS= read -r -d '' file; do \
		echo "Checking $$file"; \
		if ! clang-tidy "$$file" \
			--config-file=.clang-tidy \
			--quiet \
			--header-filter='^(src)/' \
			--system-headers=false; then \
			touch /tmp/clang-tidy-failed; \
		fi; \
	done
	@if [ -f /tmp/clang-tidy-failed ]; then \
		rm -f /tmp/clang-tidy-failed; \
		echo "❌ Lint failed: naming standard violations found"; \
		exit 1; \
	else \
		echo "✅ Lint passed"; \
	fi

.PHONY: install-jansson
install-jansson:
	git clone --branch lib --single-branch https://github.com/stockholm-3/just-weather.git ../lib

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
# ============================================================================
-include $(DEP_CLIENT)
-include $(DEP_JANSSON)
