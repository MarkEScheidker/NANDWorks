CC ?= gcc
CXX ?= g++

# -------------------------
# Logging / profiling flags
LOG_ONFI_LEVEL ?= 0
LOG_HAL_LEVEL  ?= 0
PROFILE_TIME   ?= 0
# PROFILE_DELAY_TIME is read directly in code; set it via CPPFLAGS if needed.

WITH_LUAJIT ?= 1

LUAJIT_DIR := lib/LuaJIT
LUAJIT_LIB := $(LUAJIT_DIR)/src/libluajit.a
LUAJIT_INC := $(LUAJIT_DIR)/src

THIRD_PARTY_DIR := lib/bcm2835_install

# Layout
OBJ_DIR = build
LIB_DIR = $(OBJ_DIR)/lib
BIN_DIR = bin

CPPFLAGS ?=
CPPFLAGS += -Iinclude -I$(THIRD_PARTY_DIR)/include \
            -DLOG_ONFI_LEVEL=$(LOG_ONFI_LEVEL) \
            -DLOG_HAL_LEVEL=$(LOG_HAL_LEVEL) \
            -DPROFILE_TIME=$(PROFILE_TIME)

ifeq ($(WITH_LUAJIT),1)
CPPFLAGS += -DNANDWORKS_WITH_LUAJIT=1 -I$(LUAJIT_INC)
LUAJIT_EXTRA_LIBS := $(LUAJIT_LIB)
else
CPPFLAGS += -DNANDWORKS_WITH_LUAJIT=0
LUAJIT_EXTRA_LIBS :=
endif

CXXFLAGS ?= -std=c++17 -Wall -Wextra -O3

LDFLAGS ?=
LDFLAGS += -L$(THIRD_PARTY_DIR)/lib

LDLIBS ?=
LDLIBS += -lbcm2835 -lrt

EXTRA_EXAMPLE_LIBS ?= -lpigpio
APP_LDLIBS     = $(LDLIBS) $(LUAJIT_EXTRA_LIBS)
TEST_LDLIBS    = $(LDLIBS) $(LUAJIT_EXTRA_LIBS)
EXAMPLE_LDLIBS = $(LDLIBS) $(EXTRA_EXAMPLE_LIBS) $(LUAJIT_EXTRA_LIBS)
MAIN_SOURCE = nandworks
MAIN_OBJ     = $(OBJ_DIR)/$(MAIN_SOURCE).o
MAIN_TARGET  = $(BIN_DIR)/nandworks


# Program-specific extra libraries
# Core/library sources (no app/test code)
CORE_SOURCES = microprocessor_interface timing gpio \
               onfi/init onfi/identify onfi/read onfi/program onfi/erase onfi/onfi_interface onfi/timed_commands \
               onfi/address onfi/param_page onfi/controller onfi/device onfi/device_config \
               driver/command_registry driver/driver_context driver/command_arguments driver/cli_parser \
               driver/commands/onfi_commands driver/commands/script_command \
               scripting/lua_engine

# Program source layout
APP_SOURCE_DIR     = apps
TEST_SOURCE_DIR    = tests
EXAMPLE_SOURCE_DIR = examples

# Discover application, test, and example sources automatically
APP_CPP     = $(sort $(wildcard $(APP_SOURCE_DIR)/*.cpp))
TEST_CPP    = $(sort $(wildcard $(TEST_SOURCE_DIR)/*.cpp))
EXAMPLE_CPP = $(sort $(wildcard $(EXAMPLE_SOURCE_DIR)/*.cpp))

# Derive program names (without extensions)
APP_PROGRAMS     = $(notdir $(basename $(APP_CPP)))
TEST_PROGRAMS    = $(notdir $(basename $(TEST_CPP)))
EXAMPLE_PROGRAMS = $(notdir $(basename $(EXAMPLE_CPP)))

# Objects
CORE_OBJS    = $(addprefix $(OBJ_DIR)/src/,$(addsuffix .o,$(CORE_SOURCES)))
APP_OBJS     = $(addprefix $(OBJ_DIR)/apps/,$(addsuffix .o,$(APP_PROGRAMS)))
TEST_OBJS    = $(addprefix $(OBJ_DIR)/tests/,$(addsuffix .o,$(TEST_PROGRAMS)))
EXAMPLE_OBJS = $(addprefix $(OBJ_DIR)/examples/,$(addsuffix .o,$(EXAMPLE_PROGRAMS)))

# Binaries
APP_BIN_DIR     = $(BIN_DIR)/apps
TEST_BIN_DIR    = $(BIN_DIR)/tests
EXAMPLE_BIN_DIR = $(BIN_DIR)/examples

APP_TARGETS     = $(addprefix $(APP_BIN_DIR)/,$(APP_PROGRAMS))
TEST_TARGETS    = $(addprefix $(TEST_BIN_DIR)/,$(TEST_PROGRAMS))
EXAMPLE_TARGETS = $(addprefix $(EXAMPLE_BIN_DIR)/,$(EXAMPLE_PROGRAMS))
TARGETS         = $(MAIN_TARGET) $(APP_TARGETS) $(TEST_TARGETS) $(EXAMPLE_TARGETS)

all: $(TARGETS) docs

.PHONY: debug trace profile help docs clean nandworks

debug:
	$(MAKE) all LOG_ONFI_LEVEL=4 LOG_HAL_LEVEL=4 PROFILE_TIME=1 CXXFLAGS="$(CXXFLAGS) -g -O0"

trace:
	$(MAKE) all LOG_ONFI_LEVEL=5 LOG_HAL_LEVEL=5 PROFILE_TIME=1 CXXFLAGS="$(CXXFLAGS) -g -O0"

profile:
	$(MAKE) all PROFILE_TIME=1

help:
	@echo "Build targets:"
	@echo "  make               Build library and all discovered apps/tests/examples"
	@echo "  make <name>        Build a specific program (apps/tests/examples)"
	@echo "  Applications: $(if $(APP_PROGRAMS),$(APP_PROGRAMS),<none>)"
	@echo "  Tests: $(if $(TEST_PROGRAMS),$(TEST_PROGRAMS),<none>)"
	@echo "  Examples: $(if $(EXAMPLE_PROGRAMS),$(EXAMPLE_PROGRAMS),<none>)"
	@echo
	@echo "Variables: LOG_ONFI_LEVEL, LOG_HAL_LEVEL, PROFILE_TIME"

docs:
	@command -v doxygen >/dev/null 2>&1 && doxygen docs/Doxyfile || echo "Doxygen not found; skipping docs"

# Static library with core only
$(LIB_DIR)/libonfi.a: $(CORE_OBJS) | $(LIB_DIR)
	ar rcs $@ $(CORE_OBJS)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(LIB_DIR): | $(OBJ_DIR)
	mkdir -p $(LIB_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Core object rule
$(OBJ_DIR)/src/%.o: src/%.cpp | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# App object rule
$(OBJ_DIR)/apps/%.o: $(APP_SOURCE_DIR)/%.cpp | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Test object rule
$(OBJ_DIR)/tests/%.o: $(TEST_SOURCE_DIR)/%.cpp | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Example object rule
$(OBJ_DIR)/examples/%.o: $(EXAMPLE_SOURCE_DIR)/%.cpp | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

ifeq ($(WITH_LUAJIT),1)
$(LUAJIT_LIB):
	$(MAKE) -C $(LUAJIT_DIR) BUILDMODE=static CC="$(CC)"

$(OBJ_DIR)/src/scripting/%.o: | $(LUAJIT_LIB)
$(OBJ_DIR)/src/driver/commands/script_command.o: | $(LUAJIT_LIB)
endif

$(MAIN_OBJ): $(MAIN_SOURCE).cpp | $(OBJ_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(MAIN_TARGET): $(LIB_DIR)/libonfi.a $(MAIN_OBJ) $(LUAJIT_EXTRA_LIBS)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(MAIN_OBJ) $(LIB_DIR)/libonfi.a -o $@ $(APP_LDLIBS)
	rm $(MAIN_OBJ)


# Convenience targets for direct invocation (e.g. `make tester`)
$(APP_PROGRAMS): %: $(APP_BIN_DIR)/%
$(TEST_PROGRAMS): %: $(TEST_BIN_DIR)/%
$(EXAMPLE_PROGRAMS): %: $(EXAMPLE_BIN_DIR)/%

# Link rules per program type
$(APP_BIN_DIR)/%: $(LIB_DIR)/libonfi.a $(OBJ_DIR)/apps/%.o $(LUAJIT_EXTRA_LIBS)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJ_DIR)/apps/$*.o $(LIB_DIR)/libonfi.a -o $@ $(APP_LDLIBS)
nandworks: $(MAIN_TARGET)
	@:


$(TEST_BIN_DIR)/%: $(LIB_DIR)/libonfi.a $(OBJ_DIR)/tests/%.o $(LUAJIT_EXTRA_LIBS)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJ_DIR)/tests/$*.o $(LIB_DIR)/libonfi.a -o $@ $(TEST_LDLIBS)

$(EXAMPLE_BIN_DIR)/%: $(LIB_DIR)/libonfi.a $(OBJ_DIR)/examples/%.o $(LUAJIT_EXTRA_LIBS)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJ_DIR)/examples/$*.o $(LIB_DIR)/libonfi.a -o $@ $(EXAMPLE_LDLIBS)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) docs/html main benchmark profiler erase_chip tester
ifeq ($(WITH_LUAJIT),1)
	$(MAKE) -C $(LUAJIT_DIR) clean
endif
