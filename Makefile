CXX ?= c++
CXXFLAGS ?= -O2
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic
PYTHON ?= python3
BUILD_DIR ?= build

SOURCES := src/raytracer.cpp src/camera.cpp src/image.cpp src/scene_loader.cpp
OBJECTS := $(SOURCES:src/%.cpp=$(BUILD_DIR)/obj/%.o)
DEPFILES := $(OBJECTS:.o=.d)
TARGET := $(BUILD_DIR)/ray

.PHONY: all clean test
.DELETE_ON_ERROR:

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o "$@"

$(BUILD_DIR)/obj/%.o: src/%.cpp
	@mkdir -p "$(@D)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c "$<" -o "$@"

test: $(TARGET)
	$(PYTHON) tests/smoke_test.py "$(TARGET)"

clean:
	rm -rf "$(BUILD_DIR)/obj" "$(TARGET)"

-include $(DEPFILES)
