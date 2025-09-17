CXX = g++

# -------------------------
# Logging / profiling flags
LOG_ONFI_LEVEL ?= 0
LOG_HAL_LEVEL  ?= 0
PROFILE_TIME   ?= 0

# Layout
OBJ_DIR = build
LIB_DIR = $(OBJ_DIR)/lib
BIN_DIR = bin

# Base compile flags
CXXFLAGS = -Wall -O3 -s -lrt -std=c++0x -Iinclude -I$(OBJ_DIR)/../lib/bcm2835_install/include -L$(OBJ_DIR)/../lib/bcm2835_install/lib \
           -DLOG_ONFI_LEVEL=$(LOG_ONFI_LEVEL) -DLOG_HAL_LEVEL=$(LOG_HAL_LEVEL) -DPROFILE_TIME=$(PROFILE_TIME)
LIBS = -lbcm2835

APP_LIBS     = $(LIBS)
TEST_LIBS    = $(LIBS)
EXTRA_EXAMPLE_LIBS ?= -lpigpio
EXAMPLE_LIBS = $(LIBS) $(EXTRA_EXAMPLE_LIBS)

# Program-specific extra libraries
# Core/library sources (no app/test code)
CORE_SOURCES = microprocessor_interface timing gpio \
               onfi/init onfi/identify onfi/read onfi/program onfi/erase onfi/util \
               onfi/address onfi/param_page onfi/controller onfi/device onfi/data_sink

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
TARGETS         = $(APP_TARGETS) $(TEST_TARGETS) $(EXAMPLE_TARGETS)

all: $(TARGETS) docs

.PHONY: debug trace profile help docs clean

debug:
	$(MAKE) all LOG_ONFI_LEVEL=4 LOG_HAL_LEVEL=4 PROFILE_TIME=1 CXXFLAGS="$(CXXFLAGS) -g -O2"

trace:
	$(MAKE) all LOG_ONFI_LEVEL=5 LOG_HAL_LEVEL=5 PROFILE_TIME=1 CXXFLAGS="$(CXXFLAGS) -g -O2"

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
	$(CXX) -c $< $(CXXFLAGS) -o $@

# App object rule
$(OBJ_DIR)/apps/%.o: $(APP_SOURCE_DIR)/%.cpp | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CXX) -c $< $(CXXFLAGS) -o $@

# Test object rule
$(OBJ_DIR)/tests/%.o: $(TEST_SOURCE_DIR)/%.cpp | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CXX) -c $< $(CXXFLAGS) -o $@

# Example object rule
$(OBJ_DIR)/examples/%.o: $(EXAMPLE_SOURCE_DIR)/%.cpp | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CXX) -c $< $(CXXFLAGS) -o $@

# Convenience targets for direct invocation (e.g. `make tester`)
$(APP_PROGRAMS): %: $(APP_BIN_DIR)/%
$(TEST_PROGRAMS): %: $(TEST_BIN_DIR)/%
$(EXAMPLE_PROGRAMS): %: $(EXAMPLE_BIN_DIR)/%

# Link rules per program type
$(APP_BIN_DIR)/%: $(LIB_DIR)/libonfi.a $(OBJ_DIR)/apps/%.o
	mkdir -p $(dir $@)
	$(CXX) $(OBJ_DIR)/apps/$*.o $(CXXFLAGS) $(LIB_DIR)/libonfi.a $(APP_LIBS) -o $@

$(TEST_BIN_DIR)/%: $(LIB_DIR)/libonfi.a $(OBJ_DIR)/tests/%.o
	mkdir -p $(dir $@)
	$(CXX) $(OBJ_DIR)/tests/$*.o $(CXXFLAGS) $(LIB_DIR)/libonfi.a $(TEST_LIBS) -o $@

$(EXAMPLE_BIN_DIR)/%: $(LIB_DIR)/libonfi.a $(OBJ_DIR)/examples/%.o
	mkdir -p $(dir $@)
	$(CXX) $(OBJ_DIR)/examples/$*.o $(CXXFLAGS) $(LIB_DIR)/libonfi.a $(EXAMPLE_LIBS) -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) docs/html main benchmark profiler erase_chip tester
