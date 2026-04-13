cmake_minimum_required(VERSION 3.14)
project(MexForge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only library
add_library(mexforge INTERFACE)
target_include_directories(mexforge INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Optional: Find MATLAB for building examples
option(MEXFORGE_BUILD_EXAMPLES "Build example MEX libraries" OFF)

if(MEXFORGE_BUILD_EXAMPLES)
    find_package(Matlab REQUIRED COMPONENTS MX_LIBRARY)
    add_subdirectory(examples/simple_math)
endif()
