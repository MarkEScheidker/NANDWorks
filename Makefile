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

# Core/library sources (no app/test code)
CORE_SOURCES = microprocessor_interface timing gpio \
               onfi/init onfi/identify onfi/read onfi/program onfi/erase onfi/util \
               onfi/address onfi/param_page onfi/controller onfi/device onfi/data_sink

# App sources (live in apps/)
APP_SOURCES = test_runner erase_chip benchmark profiler

# Objects
CORE_OBJS = $(addprefix $(OBJ_DIR)/src/,$(addsuffix .o,$(CORE_SOURCES)))
APP_OBJS  = $(addprefix $(OBJ_DIR)/apps/,$(addsuffix .o,$(APP_SOURCES)))

# Binaries
TARGETS = $(BIN_DIR)/tester $(BIN_DIR)/erase_chip $(BIN_DIR)/benchmark $(BIN_DIR)/profiler

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
	@echo "  make               Build library and apps"
	@echo "  make tester        Build the test runner"
	@echo "  make erase_chip    Build the erase utility"
	@echo "  make benchmark     Build the benchmark app"
	@echo "  make profiler      Build the profiler app"
	@echo
	@echo "Variables: LOG_ONFI_LEVEL, LOG_HAL_LEVEL, PROFILE_TIME"

docs:
	@command -v doxygen >/dev/null 2>&1 && doxygen docs/Doxyfile || echo "Doxygen not found; skipping docs"

# Static library with core only
$(LIB_DIR)/libonfi.a: $(CORE_OBJS) | $(LIB_DIR)
	ar rcs $@ $(CORE_OBJS)

$(BIN_DIR)/tester: $(LIB_DIR)/libonfi.a $(OBJ_DIR)/apps/test_runner.o | $(BIN_DIR)
	$(CXX) $(OBJ_DIR)/apps/test_runner.o $(CXXFLAGS) $(LIB_DIR)/libonfi.a $(LIBS) -o $@

$(BIN_DIR)/erase_chip: $(LIB_DIR)/libonfi.a $(OBJ_DIR)/apps/erase_chip.o | $(BIN_DIR)
	$(CXX) $(OBJ_DIR)/apps/erase_chip.o $(CXXFLAGS) $(LIB_DIR)/libonfi.a $(LIBS) -o $@

$(BIN_DIR)/benchmark: $(LIB_DIR)/libonfi.a $(OBJ_DIR)/apps/benchmark.o | $(BIN_DIR)
	$(CXX) $(OBJ_DIR)/apps/benchmark.o $(CXXFLAGS) $(LIB_DIR)/libonfi.a $(LIBS) -o $@

$(BIN_DIR)/profiler: $(LIB_DIR)/libonfi.a $(OBJ_DIR)/apps/profiler.o | $(BIN_DIR)
	$(CXX) $(OBJ_DIR)/apps/profiler.o $(CXXFLAGS) $(LIB_DIR)/libonfi.a $(LIBS) -o $@

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
$(OBJ_DIR)/apps/%.o: apps/%.cpp | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CXX) -c $< $(CXXFLAGS) -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) main benchmark profiler erase_chip tester
