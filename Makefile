CXX = g++
CXXFLAGS = -Wall -O0 -lrt -std=c++0x -Iinclude
OBJ_DIR = build
TARGET = main

SOURCES = main onfi_head microprocessor_interface
OBJS = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(SOURCES)))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(CXXFLAGS) -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: src/%.cpp | $(OBJ_DIR)
	$(CXX) -c $< $(CXXFLAGS) -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
