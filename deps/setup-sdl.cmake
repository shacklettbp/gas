set(SDL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdl-src")
set(SDL_BUNDLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bundled-sdl")
set(SDL_BUILD_TYPE "Release")

set(BUNDLE_TMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bundle-tmp")
set(SDL_BUILD_DIR "${BUNDLE_TMP_DIR}/sdl-build")
set(SDL_BUILD_TIMESTAMP_FILE "${BUNDLE_TMP_DIR}/sdl-build-stamp")
set(SDL_BUILD_CONFIG_HASH_FILE "${BUNDLE_TMP_DIR}/sdl-build-config-hash")

function(fetch_build_sdl)

  set(FETCHCONTENT_BASE_DIR "${BUNDLE_TMP_DIR}")
  set(FETCHCONTENT_QUIET FALSE)
  FetchContent_Declare(sdl-bundled
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG 9ff3446f036094bc005ef119e0cf07fc9b503b8e 
    SOURCE_DIR "${SDL_SRC_DIR}"
    GIT_PROGRESS ON
  )

  FetchContent_MakeAvailable(sdl-bundled)
  set(FETCHCONTENT_QUIET TRUE)

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
