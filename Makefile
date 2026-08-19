# Chess Engine Makefile with NNUE support

CXX = g++
CXXFLAGS = -std=c++17 -O3 -march=native -flto -DNDEBUG
CXXFLAGS += -I./include
LDFLAGS = -flto

# Source files
SRCS = src/position.cpp \
       src/movegen.cpp \
       src/search.cpp \
       src/evaluation.cpp \
       src/tt.cpp \
       src/uci.cpp \
       src/bitboard.cpp \
       src/nnue_types.cpp \
       src/nnue_feature_transformer.cpp \
       src/nnue_network.cpp \
       src/nnue_weight_initializer.cpp \
       src/nnue_accumulator_cache.cpp \
       src/nnue_evaluation.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Target executable
TARGET = chess_engine

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

# Compile
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(OBJS) $(TARGET)

# Debug build
debug: CXXFLAGS = -std=c++17 -g -O0 -I./include -DDEBUG
debug: clean all

# Profile build
profile: CXXFLAGS = -std=c++17 -O3 -march=native -pg -I./include
profile: LDFLAGS = -pg
profile: clean all

.PHONY: all clean debug profile
