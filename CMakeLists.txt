list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if (NOT PROJECT_NAME)
  cmake_minimum_required(VERSION 3.26 FATAL_ERROR)
  cmake_policy(VERSION 3.26)
  
  set(GAS_MADRONA_DIR "" CACHE STRING "Directory containing madrona")
  
  if (NOT GAS_MADRONA_DIR)
    message(FATAL_ERROR "Must set GAS_MADRONA_DIR")
  endif()
  
  include("${GAS_MADRONA_DIR}/cmake/madrona_init.cmake")

  list(APPEND LANGS C CXX)
  if (APPLE)
    list(APPEND LANGS OBJC)
  endif()

  project(gas LANGUAGES ${LANGS})
  
  include(setup)
  include(dependencies)
  
  SET(MADRONA_ENABLE_TESTS TRUE)
  add_subdirectory("${GAS_MADRONA_DIR}" madrona-standalone EXCLUDE_FROM_ALL)
endif()

add_subdirectory(deps)

include(gas-find-deps)

list(APPEND GAS_CORE_SOURCES
  gas.hpp gas.inl gas.cpp
  backend_common.hpp 
  init.hpp init.cpp
  mem.hpp mem.cpp
  linux.hpp windows.hpp
)

if (GAS_USE_DAWN)
  list(APPEND GAS_CORE_SOURCES
    wgpu.hpp wgpu.cpp wgpu_init.hpp
  )
endif()

add_library(gas_core STATIC
  ${GAS_CORE_SOURCES}
)

target_compile_definitions(gas_core PUBLIC 
  GAS_SHADER_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/\"
)

target_link_libraries(gas_core PRIVATE
  madrona_common
)

if (GAS_USE_DAWN)
  target_link_libraries(gas_core PRIVATE gas_dawn)
endif()

add_library(gas_ui STATIC
  gas_ui.hpp gas_ui.inl gas_ui.cpp
)

target_link_libraries(gas_ui PRIVATE
  gas_core madrona_common
)

if (GAS_USE_SDL)
  target_link_libraries(gas_ui PRIVATE gas_sdl)
endif()

add_library(gas_imgui STATIC
  gas_imgui.hpp gas_imgui.cpp
)

target_link_libraries(gas_imgui
  PUBLIC
    gas_imgui_hdrs
  PRIVATE
    gas_core gas_ui gas_imgui_impl madrona_common
)

add_library(gas_shader_compiler SHARED
  shader_compiler.hpp shader_compiler.inl shader_compiler.cpp
)
target_link_libraries(gas_shader_compiler PRIVATE
  gas_slang
  madrona_common
)

function(gas_link_hermetic_libcxx tgt)
  if (WIN32)
    return()
  endif()

  FetchContent_GetProperties(MadronaBundledToolchain)

  set(HERMETIC_LIBCXX_PATH "${madronabundledtoolchain_SOURCE_DIR}/libcxx-hermetic")

  target_compile_options(${tgt} PRIVATE -nostdinc++ -nostdlib++)
  target_link_options(${tgt} PRIVATE -nostdlib++)
  target_include_directories(${tgt} SYSTEM PRIVATE
    $<BUILD_INTERFACE:${HERMETIC_LIBCXX_PATH}/include/c++/v1>)
  target_link_libraries(${tgt} PRIVATE
    ${HERMETIC_LIBCXX_PATH}/lib/libc++-hermetic.a )
endfunction()

if (GAS_USE_DAWN)
  add_library(gas_dawn_tint SHARED
    wgpu_shader_compiler.hpp
    wgpu_shader_compiler.cpp
  )
  target_link_libraries(gas_dawn_tint PRIVATE 
    madrona_hdrs
    madrona_noexceptrtti
    gas_dawn
    gas_dawn_tint_libs
  )

  if (WIN32)
    set_property(TARGET gas_dawn_tint PROPERTY
      MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  else()
    gas_link_hermetic_libcxx(gas_dawn_tint)
  endif()

  target_link_libraries(gas_shader_compiler PRIVATE gas_dawn gas_dawn_tint)
endif()

add_executable(gas_frontend
  frontend.cpp 
)

target_link_libraries(gas_frontend
  PRIVATE 
    gas_core
    gas_ui
    gas_imgui
    madrona_common
)

add_subdirectory(test)
