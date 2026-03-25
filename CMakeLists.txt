cmake_minimum_required(VERSION 3.20)
project(kernelCAD VERSION 0.1.1 LANGUAGES CXX)

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
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGLWidgets Gui PrintSupport)
find_package(Qt6 QUIET COMPONENTS Svg)
if(COMMAND qt_standard_project_setup)
  qt_standard_project_setup()
endif()

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

# ── Installation and Packaging ────────────────────────────────────────────────
if(UNIX AND NOT APPLE)
  set(CPACK_PACKAGING_INSTALL_PREFIX "/")
  
  install(TARGETS kernelCAD kernelcad-cli DESTINATION opt/kernelcad)
  install(DIRECTORY plugins python DESTINATION opt/kernelcad)
  install(FILES README.md CHANGELOG.md LICENSE DESTINATION opt/kernelcad)
  install(FILES kernelcad.desktop DESTINATION usr/share/applications)
  install(FILES kernelcad.svg DESTINATION usr/share/icons/hicolor/scalable/apps)
  
  file(WRITE "${CMAKE_BINARY_DIR}/kernelcad.sh" "#!/bin/sh\nexec /opt/kernelcad/kernelCAD \"$@\"\n")
  install(PROGRAMS "${CMAKE_BINARY_DIR}/kernelcad.sh" DESTINATION usr/bin RENAME kernelcad)

  set(CPACK_PACKAGE_NAME "kernelcad-desktop")
  set(CPACK_PACKAGE_VENDOR "w1ne")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "kernelCAD Desktop")
  set(CPACK_PACKAGE_VERSION "0.1.0")
  set(CPACK_GENERATOR "TGZ;DEB;RPM")
  
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "w1ne")
  set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)

  include(CPack)
endif()
