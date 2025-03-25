set(SLANG_BUNDLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bundled-slang")

set(SLANG_SRC_DIR "${GAS_BUNDLE_TMP_DIR}/slang")
set(SLANG_BUILD_TIMESTAMP_FILE "${GAS_BUNDLE_TMP_DIR}/slang-build-stamp")
set(SLANG_BUILD_CONFIG_HASH_FILE "${GAS_BUNDLE_TMP_DIR}/slang-build-config-hash")

function(fetch_build_slang)
  FetchContent_Populate(slang-bundled
    GIT_REPOSITORY https://github.com/shader-slang/slang
    GIT_TAG v2025.6.3
    GIT_PROGRESS ON
    SOURCE_DIR "${SLANG_SRC_DIR}"
  )

  if (NOT WIN32) #FIX
    FetchContent_GetProperties(MadronaBundledToolchain)

    set(HERMETIC_LIBCXX_PATH "${madronabundledtoolchain_SOURCE_DIR}/libcxx-hermetic")

    set(HERMETIC_LIBCXX_INC_FLAGS "-nostdinc++ -isystem ${HERMETIC_LIBCXX_PATH}/include/c++/v1")
    set(HERMETIC_LIBCXX_LINKER_FLAGS
      "-nostdlib++ ${HERMETIC_LIBCXX_PATH}/lib/libc++-hermetic.a")

    set(SLANG_CXX_COMPILER ${CMAKE_CXX_COMPILER} ${HERMETIC_LIBCXX_INC_FLAGS} ${HERMETIC_LIBCXX_LINKER_FLAGS})
else ()
  set(SLANG_CXX_COMPILER ${CMAKE_CXX_COMPILER})
endif()

  set(SLANG_CMAKE_ARGS)

  list(APPEND SLANG_CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX=${SLANG_BUNDLED_DIR}"
    "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}"
    "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
    "-DSLANG_SLANG_LLVM_FLAVOR=DISABLE"
    "-DSLANG_LIB_TYPE=SHARED"
  )

  function(build_slang)
    find_package(Git REQUIRED)

    execute_process(COMMAND ${CMAKE_COMMAND}
      --preset default
      "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
      "-DCMAKE_CXX_COMPILER=${SLANG_CXX_COMPILER}"
      "-DCMAKE_OBJC_COMPILER=${CMAKE_OBJC_COMPILER}"
      ${SLANG_CMAKE_ARGS}
      WORKING_DIRECTORY ${SLANG_SRC_DIR}
      COMMAND_ERROR_IS_FATAL ANY
    )

    execute_process(COMMAND ${CMAKE_COMMAND}
      --build --preset release
      WORKING_DIRECTORY ${SLANG_SRC_DIR} 
      COMMAND_ERROR_IS_FATAL ANY
    )

    execute_process(COMMAND ${CMAKE_COMMAND}
      --build ${SLANG_SRC_DIR}/build --config Release --target install
      COMMAND_ERROR_IS_FATAL ANY
    )

    file(SHA512 "${CMAKE_CURRENT_LIST_FILE}" SLANG_CONFIG_FILE_HASH)

    file(TOUCH "${SLANG_BUILD_TIMESTAMP_FILE}")
    file(WRITE "${SLANG_BUILD_CONFIG_HASH_FILE}" "${SLANG_CONFIG_FILE_HASH}")
  endfunction()

  build_slang()
endfunction()

function(check_build_slang)
  file(SHA512 "${CMAKE_CURRENT_LIST_FILE}" SLANG_CONFIG_FILE_HASH)

  if (EXISTS "${SLANG_BUILD_TIMESTAMP_FILE}")
    file(READ "${SLANG_BUILD_TIMESTAMP_FILE}" CUR_BUILD_TIMESTAMP)
  else()
    set(CUR_BUILD_TIMESTAMP "")
  endif()

  if (EXISTS "${SLANG_BUILD_CONFIG_HASH_FILE}")
    file(READ "${SLANG_BUILD_CONFIG_HASH_FILE}" CUR_BUILD_CONFIG_HASH)
  else()
    set(CUR_BUILD_CONFIG_HASH "")
  endif()

  set(NEED_BUILD_SLANG FALSE)
  if (NOT "${CUR_BUILD_CONFIG_HASH}" MATCHES "${SLANG_BUILD_CONFIG_FILE_HASH}")
    set(NEED_BUILD_SLANG TRUE)
  endif()

  if (NOT EXISTS "${SLANG_BUILD_TIMESTAMP_FILE}")
    set(NEED_BUILD_SLANG TRUE)
  else()
    if ("${CMAKE_CURRENT_LIST_FILE}" IS_NEWER_THAN "${SLANG_BUILD_TIMESTAMP_FILE}")
      set(NEED_BUILD_SLANG TRUE)
    endif()
  endif()

  if (NEED_BUILD_SLANG)
    fetch_build_slang()
  endif()
endfunction()

check_build_slang()
