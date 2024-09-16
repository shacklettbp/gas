set(IMGUI_BUNDLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")

function(fetch_imgui)
  FetchContent_Populate(imgui-bundled
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG 1dfbb100d6ca6e7d813102e979c07f66a56546bb
    GIT_PROGRESS ON
    GIT_SUBMODULES ""
    GIT_SUBMODULES_RECURSE OFF
    SOURCE_DIR "${IMGUI_BUNDLED_DIR}"
  )
  set(FETCHCONTENT_QUIET TRUE)
endfunction()

function(check_fetch_imgui)
  file(SHA512 "${CMAKE_CURRENT_LIST_FILE}" IMGUI_CONFIG_FILE_HASH)

  if (EXISTS "${IMGUI_BUILD_TIMESTAMP_FILE}")
    file(READ "${IMGUI_BUILD_TIMESTAMP_FILE}" CUR_BUILD_TIMESTAMP)
  else()
    set(CUR_BUILD_TIMESTAMP "")
  endif()

  if (EXISTS "${IMGUI_BUILD_CONFIG_HASH_FILE}")
    file(READ "${IMGUI_BUILD_CONFIG_HASH_FILE}" CUR_BUILD_CONFIG_HASH)
  else()
    set(CUR_BUILD_CONFIG_HASH "")
  endif()

  set(NEED_FETCH_IMGUI FALSE)
  if (NOT "${CUR_BUILD_CONFIG_HASH}" MATCHES "${IMGUI_BUILD_CONFIG_FILE_HASH}")
    set(NEED_FETCH_IMGUI TRUE)
  endif()

  if (NOT EXISTS "${IMGUI_BUILD_TIMESTAMP_FILE}")
    set(NEED_FETCH_IMGUI TRUE)
  else()
    if ("${CMAKE_CURRENT_LIST_FILE}" IS_NEWER_THAN "${IMGUI_BUILD_TIMESTAMP_FILE}")
      set(NEED_FETCH_IMGUI TRUE)
    endif()
  endif()

  if (NEED_FETCH_IMGUI)
    fetch_imgui()
  endif()
endfunction()

check_fetch_imgui()
unset(check_fetch_imgui)
unset(fetch_imgui)
