add_library(vrowser_vim STATIC
  default_bindings.cpp
  frame_focus.cpp
  key_bindings.cpp
  key_dispatcher.cpp
  key_event.cpp
  mode_machine.cpp)
target_include_directories(vrowser_vim PUBLIC
  "${PROJECT_SOURCE_DIR}/include")
target_compile_features(vrowser_vim PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(vrowser_vim PRIVATE /W4 /permissive-)
else()
  target_compile_options(vrowser_vim PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif()
