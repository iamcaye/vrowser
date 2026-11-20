set(CEF_TARGET "vrowser")
set(VROWSER_MAC_NAME "Vrowser")
set(VERSION_SHORT "${PROJECT_VERSION}")

set(VROWSER_SHARED_SOURCES
  "${CMAKE_SOURCE_DIR}/src/core/navigation.cpp"
  "${CMAKE_SOURCE_DIR}/src/core/startup_config.cpp"
  "${CMAKE_SOURCE_DIR}/src/core/tab_list.cpp"
  "${CMAKE_SOURCE_DIR}/src/browser/cef/cef_runtime_app.cpp"
  "${CMAKE_SOURCE_DIR}/src/browser/cef/cef_browser_client.cpp"
  "${CMAKE_SOURCE_DIR}/src/browser/cef/views_delegates.cpp"
)

if(OS_MAC)
  set(VROWSER_SOURCES
    ${VROWSER_SHARED_SOURCES}
    main_mac.mm
    "${CMAKE_SOURCE_DIR}/src/browser/cef/cef_browser_client_mac.mm"
  )
  set(VROWSER_HELPER_SOURCES helper_mac.cc)
elseif(OS_LINUX)
  set(VROWSER_SOURCES
    ${VROWSER_SHARED_SOURCES}
    main_linux.cc
    "${CMAKE_SOURCE_DIR}/src/browser/cef/cef_browser_client_linux.cc"
  )
elseif(OS_WINDOWS)
  set(VROWSER_SOURCES
    ${VROWSER_SHARED_SOURCES}
    main_win.cc
    "${CMAKE_SOURCE_DIR}/src/browser/cef/cef_browser_client_win.cc"
  )
endif()

if(NOT OS_MAC)
  ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
endif()

SET_CEF_TARGET_OUT_DIR()

if(OS_LINUX)
  add_executable(${CEF_TARGET} ${VROWSER_SOURCES})
  SET_EXECUTABLE_TARGET_PROPERTIES(${CEF_TARGET})
  add_dependencies(${CEF_TARGET} libcef_dll_wrapper)
  target_link_libraries(${CEF_TARGET} libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS})
  set_target_properties(${CEF_TARGET} PROPERTIES
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CEF_TARGET_OUT_DIR}")
  COPY_FILES("${CEF_TARGET}" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
  COPY_FILES("${CEF_TARGET}" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")
  if(EXISTS "${CEF_BINARY_DIR}/libminigbm.so")
    COPY_FILES("${CEF_TARGET}" "libminigbm.so" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
  endif()
  SET_LINUX_SUID_PERMISSIONS("${CEF_TARGET}" "${CEF_TARGET_OUT_DIR}/chrome-sandbox")
endif()

if(OS_MAC)
  set(EXECUTABLE_NAME "${VROWSER_MAC_NAME}")
  set(PRODUCT_NAME "${VROWSER_MAC_NAME}")
  set(CEF_APP "${CEF_TARGET_OUT_DIR}/${VROWSER_MAC_NAME}.app")

  add_executable(${CEF_TARGET} MACOSX_BUNDLE ${VROWSER_SOURCES})
  SET_EXECUTABLE_TARGET_PROPERTIES(${CEF_TARGET})
  add_dependencies(${CEF_TARGET} libcef_dll_wrapper)
  target_link_libraries(${CEF_TARGET} libcef_dll_wrapper ${CEF_STANDARD_LIBS})
  set_target_properties(${CEF_TARGET} PROPERTIES
    OUTPUT_NAME "${VROWSER_MAC_NAME}"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/mac/Info.plist.in")
  target_compile_options(${CEF_TARGET} PRIVATE
    $<$<COMPILE_LANGUAGE:OBJCXX>:${CEF_COMPILER_FLAGS} ${CEF_CXX_COMPILER_FLAGS} -fobjc-arc>)

  COPY_MAC_FRAMEWORK("${CEF_TARGET}" "${CEF_BINARY_DIR}" "${CEF_APP}")

  foreach(suffix_list ${CEF_HELPER_APP_SUFFIXES})
    string(REPLACE ":" ";" suffix_list ${suffix_list})
    list(GET suffix_list 0 name_suffix)
    list(GET suffix_list 1 target_suffix)
    list(GET suffix_list 2 plist_suffix)

    set(helper_target "vrowser_Helper${target_suffix}")
    set(helper_output_name "Vrowser Helper${name_suffix}")
    set(helper_info_plist "${CMAKE_CURRENT_BINARY_DIR}/helper-Info${target_suffix}.plist")

    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/mac/helper-Info.plist.in" plist_contents)
    string(REPLACE "\${EXECUTABLE_NAME}" "${helper_output_name}" plist_contents "${plist_contents}")
    string(REPLACE "\${PRODUCT_NAME}" "${helper_output_name}" plist_contents "${plist_contents}")
    string(REPLACE "\${BUNDLE_ID_SUFFIX}" "${plist_suffix}" plist_contents "${plist_contents}")
    string(REPLACE "\${VERSION_SHORT}" "${VERSION_SHORT}" plist_contents "${plist_contents}")
    file(WRITE "${helper_info_plist}" "${plist_contents}")

    add_executable(${helper_target} MACOSX_BUNDLE ${VROWSER_HELPER_SOURCES})
    SET_EXECUTABLE_TARGET_PROPERTIES(${helper_target})
    add_dependencies(${helper_target} libcef_dll_wrapper)
    target_link_libraries(${helper_target} libcef_dll_wrapper ${CEF_STANDARD_LIBS})
    set_target_properties(${helper_target} PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST "${helper_info_plist}"
      OUTPUT_NAME "${helper_output_name}")
    add_dependencies(${CEF_TARGET} "${helper_target}")

    add_custom_command(
      TARGET ${CEF_TARGET}
      POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
              "${CEF_TARGET_OUT_DIR}/${helper_output_name}.app"
              "${CEF_APP}/Contents/Frameworks/${helper_output_name}.app"
      VERBATIM)
  endforeach()
endif()

if(OS_WINDOWS)
  if(USE_SANDBOX)
    add_library(${CEF_TARGET} SHARED ${VROWSER_SOURCES})
    SET_LIBRARY_TARGET_PROPERTIES(${CEF_TARGET})
    COPY_SINGLE_FILE(${CEF_TARGET} "${CEF_BINARY_DIR}/bootstrap.exe"
                     "${CEF_TARGET_OUT_DIR}/${CEF_TARGET}.exe")
  else()
    add_executable(${CEF_TARGET} WIN32 ${VROWSER_SOURCES})
    SET_EXECUTABLE_TARGET_PROPERTIES(${CEF_TARGET})
    ADD_WINDOWS_MANIFEST("${CMAKE_CURRENT_SOURCE_DIR}/win" "${CEF_TARGET}" "exe")
  endif()

  add_dependencies(${CEF_TARGET} libcef_dll_wrapper)
  target_link_libraries(${CEF_TARGET} libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS})
  COPY_FILES("${CEF_TARGET}" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
  COPY_FILES("${CEF_TARGET}" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")
  if(USE_SANDBOX)
    SET_LPAC_ACLS("${CEF_TARGET}")
  endif()
endif()

if(TARGET ${CEF_TARGET})
  target_include_directories(${CEF_TARGET} PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src")
  target_compile_features(${CEF_TARGET} PRIVATE cxx_std_20)
endif()

if(OS_MAC AND TARGET ${CEF_TARGET})
  add_custom_target(check_mac_bundle
    COMMAND "${CMAKE_SOURCE_DIR}/tools/check_mac_bundle.sh" "${CEF_APP}"
    DEPENDS ${CEF_TARGET}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Checking macOS app bundle layout"
    VERBATIM)
endif()

