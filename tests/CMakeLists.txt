include(FetchContent)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(googletest)

add_executable(vrowser_unit_tests
  unit/startup_config_test.cpp
  unit/command_registry_test.cpp
  unit/key_bindings_test.cpp
  unit/key_event_test.cpp
  unit/mode_machine_test.cpp
  unit/navigation_test.cpp
  unit/tab_list_test.cpp)
target_link_libraries(vrowser_unit_tests PRIVATE
  vrowser_core
  vrowser_commands
  vrowser_vim
  GTest::gtest_main)
target_compile_features(vrowser_unit_tests PRIVATE cxx_std_20)

include(GoogleTest)
gtest_discover_tests(vrowser_unit_tests)
