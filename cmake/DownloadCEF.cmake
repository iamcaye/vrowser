# Downloads and verifies a pinned CEF binary distribution.
# The version and SHA256 hashes live in cmake/cef_version.cmake.

function(vrowser_download_cef platform version channel download_dir)
  if(channel STREQUAL "beta")
    set(channel_part "_beta")
  else()
    set(channel_part "")
  endif()

  set(distribution "cef_binary_${version}_${platform}${channel_part}")
  set(cef_root "${download_dir}/${distribution}")
  set(CEF_ROOT "${cef_root}" CACHE INTERNAL "CEF_ROOT")

  if(IS_DIRECTORY "${cef_root}")
    message(STATUS "Using CEF distribution at ${cef_root}")
    return()
  endif()

  set(hash_var "VROWSER_CEF_SHA256_${platform}")
  if(NOT DEFINED ${hash_var} OR "${${hash_var}}" STREQUAL "")
    message(FATAL_ERROR
      "No pinned SHA256 for CEF platform '${platform}'. "
      "Add VROWSER_CEF_SHA256_${platform} to cmake/cef_version.cmake.")
  endif()
  set(expected_hash "${${hash_var}}")

  set(archive "${download_dir}/${distribution}.tar.bz2")
  set(url "https://cef-builds.spotifycdn.com/${distribution}.tar.bz2")
  string(REPLACE "+" "%2B" url "${url}")

  file(MAKE_DIRECTORY "${download_dir}")
  message(STATUS "Downloading ${url}")
  file(DOWNLOAD "${url}" "${archive}"
       EXPECTED_HASH "SHA256=${expected_hash}"
       SHOW_PROGRESS
       STATUS download_status)
  list(GET download_status 0 download_code)
  if(NOT download_code EQUAL 0)
    list(GET download_status 1 download_message)
    message(FATAL_ERROR "CEF download failed: ${download_message}")
  endif()

  message(STATUS "Extracting ${archive}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xjf "${archive}"
    WORKING_DIRECTORY "${download_dir}"
    RESULT_VARIABLE extract_code)
  if(NOT extract_code EQUAL 0)
    message(FATAL_ERROR "Failed to extract ${archive}")
  endif()
  if(NOT IS_DIRECTORY "${cef_root}")
    message(FATAL_ERROR "CEF root was not created at ${cef_root}")
  endif()
endfunction()
