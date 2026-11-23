include(FetchContent)

# Official SQLite amalgamation. The SHA3-256 is the one published on
# https://www.sqlite.org/download.html for this archive.
FetchContent_Declare(
  sqlite_amalgamation
  URL https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip
  URL_HASH SHA3_256=92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(sqlite_amalgamation)

find_package(Threads REQUIRED)

function(vrowser_add_sqlite target)
  add_library(${target} STATIC "${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c")
  target_include_directories(${target} SYSTEM PUBLIC
    "${sqlite_amalgamation_SOURCE_DIR}")
  # Extensions are never loaded, and double-quoted strings are not identifiers.
  target_compile_definitions(${target} PRIVATE
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_DQS=0
    SQLITE_DEFAULT_FOREIGN_KEYS=1
    SQLITE_OMIT_DEPRECATED)
  target_link_libraries(${target} PUBLIC Threads::Threads)
  if(UNIX AND NOT APPLE)
    target_link_libraries(${target} PUBLIC m)
  endif()
endfunction()

vrowser_add_sqlite(vrowser_sqlite)

# The CEF app links the static C runtime (/MT, /MTd), so on Windows it needs a
# SQLite built the same way. Tests keep the default dynamic runtime.
if(MSVC)
  vrowser_add_sqlite(vrowser_sqlite_static_crt)
  set_target_properties(vrowser_sqlite_static_crt PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(vrowser_storage STATIC bookmarks.cpp history.cpp migrations.cpp
  profile_database.cpp)
target_include_directories(vrowser_storage PUBLIC
  "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(vrowser_storage PRIVATE vrowser_sqlite)
target_compile_features(vrowser_storage PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(vrowser_storage PRIVATE /W4 /permissive-)
else()
  target_compile_options(vrowser_storage PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif()
