cmake_minimum_required(VERSION 3.20)
project(kernelCAD VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Build type ────────────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ── Options ───────────────────────────────────────────────────────────────────
option(KERNELCAD_BUILD_TESTS "Build tests" ON)

# ── Dependencies ──────────────────────────────────────────────────────────────

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGLWidgets Gui)
qt_standard_project_setup()

# OpenCASCADE
# Set OCCT_DIR or CMAKE_PREFIX_PATH to your OCCT installation
find_package(OpenCASCADE REQUIRED)
if(NOT OpenCASCADE_FOUND)
  message(FATAL_ERROR
    "OpenCASCADE not found.\n"
    "Set -DCMAKE_PREFIX_PATH=/path/to/occt/install or -DOpenCASCADE_DIR=...")
endif()
message(STATUS "OpenCASCADE ${OpenCASCADE_VERSION} found at ${OpenCASCADE_DIR}")

# ── Compiler options ──────────────────────────────────────────────────────────
include(cmake/CompilerOptions.cmake)

# ── Sources ───────────────────────────────────────────────────────────────────
add_subdirectory(src)

# ── Tests ─────────────────────────────────────────────────────────────────────
if(KERNELCAD_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
