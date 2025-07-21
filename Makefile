CXX = g++
CXXFLAGS = -Wall -O3 -s -lrt -std=c++0x -Iinclude -I$(OBJ_DIR)/../lib/bcm2835_install/include -L$(OBJ_DIR)/../lib/bcm2835_install/lib -lbcm2835
OBJ_DIR = build
TARGETS = main erase_chip benchmark profiler

SOURCES = main microprocessor_interface timing gpio \
         onfi/init onfi/identify onfi/read onfi/program onfi/erase onfi/util

ERASE_CHIP_SOURCES = erase_chip microprocessor_interface timing gpio \
         onfi/init onfi/identify onfi/read onfi/program onfi/erase onfi/util

BENCHMARK_SOURCES = benchmark timing gpio

PROFILER_SOURCES = profiler microprocessor_interface timing gpio \
         onfi/init onfi/identify onfi/read onfi/program onfi/erase onfi/util

OBJS = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(SOURCES)))
ERASE_CHIP_OBJS = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(ERASE_CHIP_SOURCES)))
BENCHMARK_OBJS = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(BENCHMARK_SOURCES)))
PROFILER_OBJS = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(PROFILER_SOURCES)))

all: $(TARGETS)

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
