# ========================================
# 🎵 MUSIC ANALYZER AI ALGORITHMS MAKEFILE
# ========================================

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -fPIC -DNDEBUG
INCLUDES = -I./src -I./node_modules/node-addon-api
LIBS = -lfftw3f -lm

# Source files
AI_SOURCES = src/ai_algorithms.cpp \
             src/ai_algorithms_part2.cpp \
             src/ai_algorithms_part3.cpp \
             src/ai_algorithms_master.cpp

# Object files
AI_OBJECTS = $(AI_SOURCES:.cpp=.o)

# Output
STATIC_LIB = libai_algorithms.a
SHARED_LIB = libai_algorithms.so
TEST_EXECUTABLE = test_ai_algorithms

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    SHARED_LIB = libai_algorithms.dylib
    LIBS += -framework Accelerate
    # Homebrew FFTW paths
    FFTW_PREFIX = $(shell brew --prefix fftw 2>/dev/null || echo /usr/local)
    INCLUDES += -I$(FFTW_PREFIX)/include
    LIBS += -L$(FFTW_PREFIX)/lib
else ifeq ($(UNAME_S),Linux)
    LIBS += -lpthread
endif

# Default target
all: $(STATIC_LIB) $(SHARED_LIB) test

# Compile object files
%.o: %.cpp
	@echo "🔨 Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Build static library
$(STATIC_LIB): $(AI_OBJECTS)
	@echo "📚 Building static library..."
	ar rcs $@ $(AI_OBJECTS)
	@echo "✅ Static library built: $(STATIC_LIB)"

# Build shared library
$(SHARED_LIB): $(AI_OBJECTS)
	@echo "🔗 Building shared library..."
	$(CXX) -shared $(AI_OBJECTS) $(LIBS) -o $@
	@echo "✅ Shared library built: $(SHARED_LIB)"

# Build test executable
test: test_runner.cpp $(STATIC_LIB)
	@echo "🧪 Building test executable..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) test_runner.cpp -L. -lai_algorithms $(LIBS) -o $(TEST_EXECUTABLE)
	@echo "✅ Test executable built: $(TEST_EXECUTABLE)"

# Install FFTW3 (macOS)
install-deps-mac:
	@echo "📦 Installing dependencies (macOS)..."
	brew install fftw

# Install FFTW3 (Ubuntu/Debian)
install-deps-linux:
	@echo "📦 Installing dependencies (Linux)..."
	sudo apt-get update
	sudo apt-get install libfftw3-dev

# Clean build artifacts
clean:
	@echo "🧹 Cleaning build artifacts..."
	rm -f $(AI_OBJECTS)
	rm -f $(STATIC_LIB)
	rm -f $(SHARED_LIB)
	rm -f $(TEST_EXECUTABLE)
	@echo "✅ Clean completed"

# Install library to system
install: $(SHARED_LIB)
	@echo "📦 Installing library to system..."
ifeq ($(UNAME_S),Darwin)
	cp $(SHARED_LIB) /usr/local/lib/
	cp src/ai_algorithms.h /usr/local/include/
else
	sudo cp $(SHARED_LIB) /usr/lib/
	sudo cp src/ai_algorithms.h /usr/include/
endif
	@echo "✅ Library installed"

# Run tests
run-tests: test
	@echo "🚀 Running AI algorithm tests..."
	./$(TEST_EXECUTABLE)

# Performance benchmark
benchmark: test
	@echo "⚡ Running performance benchmarks..."
	./$(TEST_EXECUTABLE) --benchmark

# Memory check (requires valgrind)
memcheck: test
	@echo "🔍 Running memory check..."
	valgrind --leak-check=full --show-leak-kinds=all ./$(TEST_EXECUTABLE)

# Node.js addon build
node-addon:
	@echo "🔗 Building Node.js addon..."
	npm run build-addon

# Full build with Node.js addon
full-build: all node-addon
	@echo "🎉 Full build completed!"

# Help
help:
	@echo "🎵 Music Analyzer AI Algorithms Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all           - Build static and shared libraries + tests"
	@echo "  clean         - Remove all build artifacts"
	@echo "  test          - Build test executable"
	@echo "  run-tests     - Build and run tests"
	@echo "  benchmark     - Run performance benchmarks"
	@echo "  install       - Install library to system"
	@echo "  node-addon    - Build Node.js addon"
	@echo "  full-build    - Complete build including Node.js addon"
	@echo ""
	@echo "Dependencies:"
	@echo "  install-deps-mac   - Install dependencies on macOS"
	@echo "  install-deps-linux - Install dependencies on Linux"
	@echo ""
	@echo "Debug:"
	@echo "  memcheck      - Run memory leak check (requires valgrind)"

.PHONY: all clean test run-tests benchmark install node-addon full-build help install-deps-mac install-deps-linux memcheck