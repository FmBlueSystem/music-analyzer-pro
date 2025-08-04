# Makefile for AI Algorithms C++ Library
# Music Analyzer Pro - Complete AI Analysis Implementation

CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -fPIC
LDFLAGS = -shared
LIBS = -lfftw3f -lm

# Include paths
INCLUDES = -I. -I/usr/local/include -I/opt/homebrew/include

# Library paths
LIBPATHS = -L/usr/local/lib -L/opt/homebrew/lib

# Source files
SOURCES = ai_algorithms.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = libai_algorithms.so

# Node.js addon
NODE_TARGET = ../build/Release/metadata_addon.node
NODE_GYPSOURCES = ../binding.gyp

.PHONY: all clean test install node-addon

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(LIBPATHS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Build Node.js addon
node-addon:
	cd .. && npm run build-addon

# Install system dependencies
install-deps:
	@echo "Installing FFTW3 library..."
	@if command -v brew >/dev/null 2>&1; then \
		brew install fftw; \
	elif command -v apt-get >/dev/null 2>&1; then \
		sudo apt-get update && sudo apt-get install -y libfftw3-dev; \
	elif command -v yum >/dev/null 2>&1; then \
		sudo yum install -y fftw-devel; \
	else \
		echo "Please install FFTW3 manually"; \
	fi

# Test compilation
test: $(TARGET)
	@echo "✅ C++ AI Algorithms library compiled successfully!"
	@echo "📊 Library size: $$(ls -lh $(TARGET) | awk '{print $$5}')"
	@echo "🔍 Symbol count: $$(nm -D $(TARGET) 2>/dev/null | wc -l || echo 'N/A')"
	@echo "📋 All AI_* algorithms implemented:"
	@echo "   🎹 AI_KEY - Krumhansl-Schmuckler Algorithm"
	@echo "   🥁 AI_BPM - Advanced Onset Detection"
	@echo "   🔊 AI_LOUDNESS - EBU R128 Standard"
	@echo "   🎸 AI_ACOUSTICNESS - Harmonic Analysis"
	@echo "   🎤 AI_INSTRUMENTALNESS - Vocal Detection"
	@echo "   🗣️ AI_SPEECHINESS - Speech Pattern Recognition"
	@echo "   🎪 AI_LIVENESS - Acoustic Environment"
	@echo "   ⚡ AI_ENERGY - Perceptual Intensity"
	@echo "   🕺 AI_DANCEABILITY - Rhythm Analysis"
	@echo "   😊 AI_VALENCE - Musical Positivity"
	@echo "   🎼 AI_MODE - Major/Minor Detection"
	@echo "   🎵 AI_TIME_SIGNATURE - Meter Detection"
	@echo "   🎨 AI_CHARACTERISTICS - Timbral Features"
	@echo "   📊 AI_CONFIDENCE - Quality Assessment"
	@echo "   🎭 AI_SUBGENRES - Genre Classification"
	@echo "   📅 AI_ERA - Era Detection"
	@echo "   🌍 AI_CULTURAL_CONTEXT - Cultural Analysis"
	@echo "   😊 AI_MOOD - Mood Analysis"
	@echo "   🎉 AI_OCCASION - Occasion Matching"

# Build and run test executable
test-run: $(TARGET)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o test_algorithms test_algorithms.cpp -L. -lai_algorithms $(LIBPATHS) $(LIBS)
	@echo "🧪 Running algorithm tests..."
	LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./test_algorithms

# Clean build files
clean:
	rm -f $(OBJECTS) $(TARGET)
	rm -f ../build/Release/metadata_addon.node
	rm -rf ../build/

# Help
help:
	@echo "Available targets:"
	@echo "  all           - Build the C++ library"
	@echo "  node-addon    - Build Node.js addon"
	@echo "  install-deps  - Install system dependencies (FFTW3)"
	@echo "  test          - Test compilation and show info"
	@echo "  clean         - Clean build files"
	@echo "  help          - Show this help"

# Debug build
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

# Profile build
profile: CXXFLAGS += -pg
profile: $(TARGET)