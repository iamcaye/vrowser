add_library(vrowser_config STATIC user_config.cpp)
target_include_directories(vrowser_config PUBLIC
  "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(vrowser_config PUBLIC vrowser_commands vrowser_vim)
target_compile_features(vrowser_config PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(vrowser_config PRIVATE /W4 /permissive-)
else()
  target_compile_options(vrowser_config PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif()
