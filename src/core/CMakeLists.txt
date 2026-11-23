add_library(vrowser_core STATIC
  navigation.cpp
  profile.cpp
  scroll.cpp
  startup_config.cpp
  tab_list.cpp
  url_input.cpp)
target_include_directories(vrowser_core PUBLIC
  "${PROJECT_SOURCE_DIR}/include")
target_compile_features(vrowser_core PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(vrowser_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(vrowser_core PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif()
