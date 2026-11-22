add_library(vrowser_ui STATIC status_line.cpp)
target_include_directories(vrowser_ui PUBLIC
  "${PROJECT_SOURCE_DIR}/include")
target_compile_features(vrowser_ui PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(vrowser_ui PRIVATE /W4 /permissive-)
else()
  target_compile_options(vrowser_ui PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif()
