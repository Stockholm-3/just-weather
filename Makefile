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
LIBS        := -lcurl

# ------------------------------------------------------------
# Source and object files
# ------------------------------------------------------------
SRC_SERVER := $(shell find -L $(SRC_DIR)/server -type f -name '*.c' ! -path "*/jansson/*") \
              $(shell find -L $(SRC_DIR)/lib -type f -name '*.c' ! -path "*/jansson/*")

SRC_CLIENT := $(shell find -L $(SRC_DIR)/client -type f -name '*.c' ! -path "*/jansson/*") \
              $(shell find -L $(SRC_DIR)/lib -type f -name '*.c' ! -path "*/jansson/*")

OBJ_SERVER  := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_SERVER))
OBJ_CLIENT  := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_CLIENT))

DEP_SERVER  := $(OBJ_SERVER:.o=.d)
DEP_CLIENT  := $(OBJ_CLIENT:.o=.d)

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
	@echo "Cleaned build artifacts."

.PHONY: format
format:
	find . \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 clang-format -i -style=file

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
