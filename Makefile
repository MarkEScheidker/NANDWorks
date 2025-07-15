CXX = g++
CXXFLAGS = -Wall -O3 -s -lrt -std=c++0x -Iinclude -lpigpio
OBJ_DIR = build
TARGETS = main erase_chip

SOURCES = main microprocessor_interface \
         onfi/init onfi/identify onfi/read onfi/program onfi/erase onfi/util

ERASE_CHIP_SOURCES = erase_chip microprocessor_interface \
         onfi/init onfi/identify onfi/read onfi/program onfi/erase onfi/util

OBJS = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(SOURCES)))
ERASE_CHIP_OBJS = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(ERASE_CHIP_SOURCES)))

all: $(TARGETS)

main: $(OBJS)
	$(CXX) $(OBJS) $(CXXFLAGS) -o $@

erase_chip: $(ERASE_CHIP_OBJS)
	$(CXX) $(ERASE_CHIP_OBJS) $(CXXFLAGS) -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: src/%.cpp | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CXX) -c $< $(CXXFLAGS) -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGETS)
