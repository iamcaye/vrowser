add_library(vrowser_commands STATIC command_registry.cpp)
target_include_directories(vrowser_commands PUBLIC
  "${PROJECT_SOURCE_DIR}/include")
target_compile_features(vrowser_commands PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(vrowser_commands PRIVATE /W4 /permissive-)
else()
  target_compile_options(vrowser_commands PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif()
