CXX = g++

# -------------------------
# Logging / profiling flags
# Configure at make-time, e.g.:
#   make LOG_ONFI_LEVEL=3 LOG_HAL_LEVEL=4 PROFILE_TIME=1
# or use convenience targets: `make debug`, `make trace`, `make profile`
# Defaults are zero-overhead (no logging, no profiling)
LOG_ONFI_LEVEL ?= 0
LOG_HAL_LEVEL  ?= 0
PROFILE_TIME   ?= 0

# Base compile flags (keep existing layout)
CXXFLAGS = -Wall -O3 -s -lrt -std=c++0x -Iinclude -I$(OBJ_DIR)/../lib/bcm2835_install/include -L$(OBJ_DIR)/../lib/bcm2835_install/lib -lbcm2835 \
           -DLOG_ONFI_LEVEL=$(LOG_ONFI_LEVEL) -DLOG_HAL_LEVEL=$(LOG_HAL_LEVEL) -DPROFILE_TIME=$(PROFILE_TIME)
OBJ_DIR = build
TARGETS = main erase_chip benchmark profiler

SOURCES = main microprocessor_interface timing gpio \
         onfi/init onfi/identify onfi/read onfi/program onfi/erase onfi/util \
         onfi/address onfi/param_page onfi/controller onfi/device onfi/data_sink

ERASE_CHIP_SOURCES = erase_chip microprocessor_interface timing gpio \
         onfi/init onfi/identify onfi/read onfi/program onfi/erase onfi/util \
         onfi/address onfi/param_page onfi/controller onfi/device onfi/data_sink

BENCHMARK_SOURCES = benchmark timing gpio

PROFILER_SOURCES = profiler microprocessor_interface timing gpio \
         onfi/init onfi/identify onfi/read onfi/program onfi/erase onfi/util \
         onfi/address onfi/param_page onfi/controller onfi/device onfi/data_sink

OBJS = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(SOURCES)))
ERASE_CHIP_OBJS = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(ERASE_CHIP_SOURCES)))
BENCHMARK_OBJS = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(BENCHMARK_SOURCES)))
PROFILER_OBJS = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(PROFILER_SOURCES)))

all: $(TARGETS) docs

.PHONY: debug trace profile help

# Enable INFO/DEBUG logs and profiling; use -g and -O2 for easier debugging
debug:
	$(MAKE) all LOG_ONFI_LEVEL=4 LOG_HAL_LEVEL=4 PROFILE_TIME=1 CXXFLAGS="$(CXXFLAGS) -g -O2"

# Enable TRACE (very verbose) logs and profiling
trace:
	$(MAKE) all LOG_ONFI_LEVEL=5 LOG_HAL_LEVEL=5 PROFILE_TIME=1 CXXFLAGS="$(CXXFLAGS) -g -O2"

# Enable only profiling I/O while keeping logs at defaults
profile:
	$(MAKE) all PROFILE_TIME=1

help:
	@echo "Build targets:"
	@echo "  make               Build all targets (logs off, profiling off by default)"
	@echo "  make debug         Enable ONFI=INFO, HAL=DEBUG logs and profiling (-g -O2)"
	@echo "  make trace         Enable ONFI/HAL TRACE logs and profiling (-g -O2)"
	@echo "  make profile       Enable profiling I/O only"
	@echo
	@echo "Variables:"
	@echo "  LOG_ONFI_LEVEL     0..5 (default 0)"
	@echo "  LOG_HAL_LEVEL      0..5 (default 0)"
	@echo "  PROFILE_TIME       0 or 1 (default 0)"

.PHONY: docs
docs:
	@command -v doxygen >/dev/null 2>&1 && doxygen docs/Doxyfile || echo "Doxygen not found; skipping docs"

main: $(OBJS)
	$(CXX) $(OBJS) $(CXXFLAGS) -o $@

erase_chip: $(ERASE_CHIP_OBJS)
	$(CXX) $(ERASE_CHIP_OBJS) $(CXXFLAGS) -o $@

benchmark: $(BENCHMARK_OBJS)
	$(CXX) $(BENCHMARK_OBJS) $(CXXFLAGS) -o $@

profiler: $(PROFILER_OBJS)
	$(CXX) $(PROFILER_OBJS) $(CXXFLAGS) -o $@


$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: src/%.cpp | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CXX) -c $< $(CXXFLAGS) -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGETS)
