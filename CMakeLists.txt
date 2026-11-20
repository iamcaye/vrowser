cmake_minimum_required(VERSION 3.24)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

if(APPLE)
  project(vrowser
    VERSION 0.1.0
    DESCRIPTION "Keyboard-first web browser built on CEF"
    LANGUAGES C CXX OBJC OBJCXX)
else()
  project(vrowser
    VERSION 0.1.0
    DESCRIPTION "Keyboard-first web browser built on CEF"
    LANGUAGES C CXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(VROWSER_WITH_CEF "Download CEF and build the desktop shell" ON)
option(VROWSER_BUILD_TESTS "Build GoogleTest unit tests" ON)

add_subdirectory(src/core)
add_subdirectory(src/commands)
add_subdirectory(src/vim)

if(VROWSER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(VROWSER_WITH_CEF)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/VrowserCef.cmake")
  add_subdirectory(apps/desktop)
endif()
