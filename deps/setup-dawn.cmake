set(DAWN_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dawn-src")
set(DAWN_BUNDLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bundled-dawn")
set(DAWN_BUILD_TYPE "Release")

set(BUNDLE_TMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bundle-tmp")
set(DAWN_BUILD_DIR "${BUNDLE_TMP_DIR}/dawn-build")
set(DAWN_BUILD_TIMESTAMP_FILE "${BUNDLE_TMP_DIR}/dawn-build-stamp")
set(DAWN_BUILD_CONFIG_HASH_FILE "${BUNDLE_TMP_DIR}/dawn-build-config-hash")

function(fetch_build_dawn)
  set(FETCHCONTENT_BASE_DIR "${BUNDLE_TMP_DIR}")
  set(FETCHCONTENT_QUIET FALSE)
  FetchContent_Populate(dawn-bundled
    GIT_REPOSITORY https://dawn.googlesource.com/dawn
    GIT_TAG 40cf7fd7bc06f871fc5e482338dffa3a8ba3acfb
    GIT_PROGRESS ON
    GIT_SUBMODULES ""
    GIT_SUBMODULES_RECURSE OFF
    SOURCE_DIR "${DAWN_SRC_DIR}"
  )
  set(FETCHCONTENT_QUIET TRUE)

  if (NOT WIN32) #FIX
    FetchContent_GetProperties(MadronaBundledToolchain)

    set(HERMETIC_LIBCXX_PATH "${madronabundledtoolchain_SOURCE_DIR}/libcxx-hermetic")
    set(OP_NEWDEL_PATH "${madronabundledtoolchain_SOURCE_DIR}/toolchain-standalone-op-newdel/lib/libmadrona_toolchain_standalone_op_newdel.a")

    set(HERMETIC_LIBCXX_INC_FLAGS "-nostdinc++ -isystem ${HERMETIC_LIBCXX_PATH}/include/c++/v1")
    set(HERMETIC_LIBCXX_LINKER_FLAGS
        "-nostdlib++ ${HERMETIC_LIBCXX_PATH}/lib/libc++-hermetic.a ${OP_NEWDEL_PATH}")
else ()
    set(HERMETIC_LIBCXX_INC_FLAGS "")
    set(HERMETIC_LIBCXX_LINKER_FLAGS "")
endif()

  set(DAWN_CXX_FLAGS "${HERMETIC_LIBCXX_INC_FLAGS}")

  set(DAWN_CMAKE_ARGS)

  set(DAWN_USE_VULKAN OFF)
  if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND DAWN_CMAKE_ARGS
      "-DDAWN_USE_WAYLAND=ON"
      "-DDAWN_USE_X11=ON"
    )

    set(DAWN_USE_VULKAN ON)
  endif()

  if (WIN32)
    list(APPEND DAWN_CMAKE_ARGS
      "-DDAWN_ENABLE_D3D11=OFF"
      "-DDAWN_ENABLE_D3D12=ON"
    )
  endif()

  if (DAWN_USE_VULKAN)
    list(APPEND DAWN_CMAKE_ARGS
      "-DDAWN_ENABLE_VULKAN=ON"
    )

    # Dawn is missing a cmake option for this, the GN files just set these defines
    set(DAWN_CXX_FLAGS "${DAWN_CXX_FLAGS} -DDAWN_ENABLE_VULKAN_VALIDATION_LAYERS=1 -DDAWN_VK_DATA_DIR=\"\\\"vulkandata\\\"\"")
  else()
    list(APPEND DAWN_CMAKE_ARGS
      "-DDAWN_ENABLE_VULKAN=OFF"
    )
  endif()

  list(APPEND DAWN_CMAKE_ARGS
    "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DCMAKE_OBJC_COMPILER=${CMAKE_OBJC_COMPILER}"
    "-DCMAKE_CXX_FLAGS=${DAWN_CXX_FLAGS}"
    "-DCMAKE_EXE_LINKER_FLAGS=${HERMETIC_LIBCXX_LINKER_FLAGS}"
    "-DCMAKE_SHARED_LINKER_FLAGS=${HERMETIC_LIBCXX_LINKER_FLAGS}"
    "-DCMAKE_MODULE_LINKER_FLAGS=${HERMETIC_LIBCXX_LINKER_FLAGS}"
    "-DCMAKE_INSTALL_PREFIX=${DAWN_BUNDLED_DIR}"
    "-DCMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION}"
    "-DCMAKE_BUILD_TYPE=${DAWN_BUILD_TYPE}"
    "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}"
    "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
    "-DDAWN_ENABLE_INSTALL=ON"
    "-DDAWN_USE_GLFW=OFF"
    "-DDAWN_ENABLE_NULL=OFF"
    "-DDAWN_BUILD_SAMPLES=OFF"
    "-DDAWN_ENABLE_DESKTOP_GL=OFF"
    "-DDAWN_ENABLE_OPENGLES=OFF"
    "-DTINT_ENABLE_INSTALL=ON"
    # For some reason the build breaks without this on undefined tint symbols
    "-DTINT_BUILD_HLSL_WRITER=ON"
    "-DTINT_BUILD_SPV_READER=ON"
    "-DTINT_BUILD_SYNTAX_TREE_WRITER=OFF"
  )

  function(build_dawn)
    find_package(Python 3.9 COMPONENTS Interpreter REQUIRED)
    find_package(Git REQUIRED)

    execute_process(COMMAND ${Python_EXECUTABLE}
      "${DAWN_SRC_DIR}/tools/fetch_dawn_dependencies.py"
      WORKING_DIRECTORY "${DAWN_SRC_DIR}"
    )

file(CONFIGURE OUTPUT "${BUNDLE_TMP_DIR}/dawn-patch" NEWLINE_STYLE UNIX @ONLY CONTENT
[=[diff --git a/src/tint/CMakeLists.txt b/src/tint/CMakeLists.txt
index 61f4f4d2d4..041e30eff6 100644
--- a/src/tint/CMakeLists.txt
+++ b/src/tint/CMakeLists.txt
@@ -731,4 +731,8 @@ if (TINT_ENABLE_INSTALL)
       get_filename_component(TINT_HEADER_DIR ${TINT_HEADER_FILE} DIRECTORY)
       install(FILES ${TINT_ROOT_SOURCE_DIR}/${TINT_HEADER_FILE}  DESTINATION  ${CMAKE_INSTALL_INCLUDEDIR}/src/tint/${TINT_HEADER_DIR})
   endforeach ()
+
+  if (WIN32)
+    install(TARGETS SPIRV-Tools-static SPIRV-Tools-opt DESTINATION ${CMAKE_INSTALL_LIBDIR})
+  endif()
 endif()
]=])

    execute_process(COMMAND
      ${GIT_EXECUTABLE} checkout 
          "src/tint/CMakeLists.txt"
      WORKING_DIRECTORY "${DAWN_SRC_DIR}"
      COMMAND_ERROR_IS_FATAL ANY
    )

    execute_process(COMMAND
      ${GIT_EXECUTABLE} apply "${BUNDLE_TMP_DIR}/dawn-patch"
      WORKING_DIRECTORY "${DAWN_SRC_DIR}"
      COMMAND_ERROR_IS_FATAL ANY
    )

    # Need to update abseil to fix a macos bug
    execute_process(COMMAND ${GIT_EXECUTABLE} checkout 1b7ed5a1932647009b72fdad8e0e834d55cf40d8
      WORKING_DIRECTORY "${DAWN_SRC_DIR}/third_party/abseil-cpp")

    execute_process(COMMAND ${CMAKE_COMMAND}
      -S "${DAWN_SRC_DIR}"
      -B "${DAWN_BUILD_DIR}"
      -G ${CMAKE_GENERATOR}
      ${DAWN_CMAKE_ARGS}
      COMMAND_ERROR_IS_FATAL ANY
    )

    execute_process(COMMAND ${CMAKE_COMMAND}
      --build "${DAWN_BUILD_DIR}"
      COMMAND_ERROR_IS_FATAL ANY
    )

    execute_process(COMMAND ${CMAKE_COMMAND}
      --install "${DAWN_BUILD_DIR}"
      COMMAND_ERROR_IS_FATAL ANY
    )

    # Tint file is missing
    file(INSTALL "${DAWN_SRC_DIR}/src/utils/compiler.h"
      DESTINATION "${DAWN_BUNDLED_DIR}/include/src/utils")

    file(SHA512 "${CMAKE_CURRENT_LIST_FILE}" DAWN_CONFIG_FILE_HASH)

    file(TOUCH "${DAWN_BUILD_TIMESTAMP_FILE}")
    file(WRITE "${DAWN_BUILD_CONFIG_HASH_FILE}" "${DAWN_CONFIG_FILE_HASH}")
  endfunction()

  build_dawn()
endfunction()

function(check_build_dawn)
  file(SHA512 "${CMAKE_CURRENT_LIST_FILE}" DAWN_CONFIG_FILE_HASH)

  if (EXISTS "${DAWN_BUILD_TIMESTAMP_FILE}")
    file(READ "${DAWN_BUILD_TIMESTAMP_FILE}" CUR_BUILD_TIMESTAMP)
  else()
    set(CUR_BUILD_TIMESTAMP "")
  endif()

  if (EXISTS "${DAWN_BUILD_CONFIG_HASH_FILE}")
    file(READ "${DAWN_BUILD_CONFIG_HASH_FILE}" CUR_BUILD_CONFIG_HASH)
  else()
    set(CUR_BUILD_CONFIG_HASH "")
  endif()

  set(NEED_BUILD_DAWN FALSE)
  if (NOT "${CUR_BUILD_CONFIG_HASH}" MATCHES "${DAWN_BUILD_CONFIG_FILE_HASH}")
    set(NEED_BUILD_DAWN TRUE)
  endif()

  if (NOT EXISTS "${DAWN_BUILD_TIMESTAMP_FILE}")
    set(NEED_BUILD_DAWN TRUE)
  else()
    if ("${CMAKE_CURRENT_LIST_FILE}" IS_NEWER_THAN "${DAWN_BUILD_TIMESTAMP_FILE}")
      set(NEED_BUILD_DAWN TRUE)
    endif()
  endif()

  if (NEED_BUILD_DAWN)
    fetch_build_dawn()
  endif()
endfunction()

check_build_dawn()
