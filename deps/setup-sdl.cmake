set(SDL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdl-src")
set(SDL_BUNDLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bundled-sdl")
set(SDL_BUILD_TYPE "Release")

set(SDL_BUILD_DIR "${GAS_BUNDLE_TMP_DIR}/sdl-build")
set(SDL_BUILD_TIMESTAMP_FILE "${GAS_BUNDLE_TMP_DIR}/sdl-build-stamp")
set(SDL_BUILD_CONFIG_HASH_FILE "${GAS_BUNDLE_TMP_DIR}/sdl-build-config-hash")

function(fetch_build_sdl)
  FetchContent_Populate(sdl-bundled
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG 89c6bc5f5022e71433a9e4eb1a2edc6d79be71f2
    GIT_PROGRESS ON
    SOURCE_DIR "${SDL_SRC_DIR}"
  )

  list(APPEND SDL_CMAKE_ARGS
    "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DCMAKE_OBJC_COMPILER=${CMAKE_OBJC_COMPILER}"
    "-DCMAKE_INSTALL_PREFIX=${SDL_BUNDLED_DIR}"
    "-DCMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION}"
    "-DCMAKE_BUILD_TYPE=${SDL_BUILD_TYPE}"
    "-DSDL_SHARED=ON"
    "-DSDL_STATIC=OFF"
    "-DSDL_ASSERTIONS=release"
    "-DSDL_KMSDRM=OFF"
    "-DSDL_OPENGLES=OFF"
    "-DSDL_DISKAUDIO=OFF"
    "-DSDL_WAYLAND=ON"
    "-DSDL_X11=ON"
    "-DSDL_OPENGL=ON"
    "-DSDL_VULKAN=ON"
    "-DSDL_METAL=ON"
  )

  function(build_sdl)
    execute_process(COMMAND ${CMAKE_COMMAND}
      -S "${SDL_SRC_DIR}"
      -B "${SDL_BUILD_DIR}"
      -G ${CMAKE_GENERATOR}
      ${SDL_CMAKE_ARGS}
      COMMAND_ERROR_IS_FATAL ANY
    )

    execute_process(COMMAND ${CMAKE_COMMAND}
      --build "${SDL_BUILD_DIR}"
      COMMAND_ERROR_IS_FATAL ANY
    )

    execute_process(COMMAND ${CMAKE_COMMAND}
      --install "${SDL_BUILD_DIR}"
      COMMAND_ERROR_IS_FATAL ANY
    )

    file(SHA512 "${CMAKE_CURRENT_LIST_FILE}" SDL_CONFIG_FILE_HASH)

    file(TOUCH "${SDL_BUILD_TIMESTAMP_FILE}")
    file(WRITE "${SDL_BUILD_CONFIG_HASH_FILE}" "${SDL_CONFIG_FILE_HASH}")
  endfunction()

  build_sdl()
endfunction()

function(check_build_sdl)
  file(SHA512 "${CMAKE_CURRENT_LIST_FILE}" SDL_CONFIG_FILE_HASH)

  if (EXISTS "${SDL_BUILD_TIMESTAMP_FILE}")
    file(READ "${SDL_BUILD_TIMESTAMP_FILE}" CUR_BUILD_TIMESTAMP)
  else()
    set(CUR_BUILD_TIMESTAMP "")
  endif()

  if (EXISTS "${SDL_BUILD_CONFIG_HASH_FILE}")
    file(READ "${SDL_BUILD_CONFIG_HASH_FILE}" CUR_BUILD_CONFIG_HASH)
  else()
    set(CUR_BUILD_CONFIG_HASH "")
  endif()

  set(NEED_BUILD_SDL FALSE)
  if (NOT "${CUR_BUILD_CONFIG_HASH}" MATCHES "${SDL_BUILD_CONFIG_FILE_HASH}")
    set(NEED_BUILD_SDL TRUE)
  endif()

  if (NOT EXISTS "${SDL_BUILD_TIMESTAMP_FILE}")
    set(NEED_BUILD_SDL TRUE)
  else()
    if ("${CMAKE_CURRENT_LIST_FILE}" IS_NEWER_THAN "${SDL_BUILD_TIMESTAMP_FILE}")
      set(NEED_BUILD_SDL TRUE)
    endif()
  endif()

  if (NEED_BUILD_SDL)
    fetch_build_sdl()
  endif()
endfunction()

check_build_sdl()
