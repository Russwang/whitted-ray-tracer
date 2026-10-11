cmake_minimum_required(VERSION 3.16)
project(WhittedRayTracer VERSION 1.0 LANGUAGES CXX)

add_executable(ray
    src/raytracer.cpp
    src/camera.cpp
    src/image.cpp
    src/scene_loader.cpp
)
target_compile_features(ray PRIVATE cxx_std_17)
set_target_properties(ray PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(ray PRIVATE /W4)
else()
    target_compile_options(ray PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME renderer-smoke
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke_test.py"
                "$<TARGET_FILE:ray>"
        )
        set_tests_properties(renderer-smoke PROPERTIES TIMEOUT 120)
    endif()
endif()
