list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if (NOT PROJECT_NAME)
  cmake_minimum_required(VERSION 3.30 FATAL_ERROR)
  cmake_policy(VERSION 3.30)
  
  set(GAS_MADRONA_DIR "" CACHE STRING "Directory containing madrona")
  
  if (NOT GAS_MADRONA_DIR)
    message(FATAL_ERROR "Must set GAS_MADRONA_DIR")
  endif()
  
  include("${GAS_MADRONA_DIR}/cmake/madrona_init.cmake")
  
  project(gas LANGUAGES C CXX OBJC)
  
  include(setup)
  include(dependencies)
  
  SET(MADRONA_ENABLE_TESTS TRUE)
  add_subdirectory("${GAS_MADRONA_DIR}" madrona-standalone)
endif()

add_subdirectory(deps)

include(gas-setup-deps)

list(APPEND GAS_CORE_SOURCES
  gas.hpp gas.inl gas.cpp
  backend_common.hpp 
  init.hpp init.cpp
  mem.hpp mem.cpp
  linux.hpp
)

if (GAS_USE_DAWN)
  list(APPEND GAS_CORE_SOURCES
    wgpu.hpp wgpu.cpp wgpu_init.hpp
  )
endif()

add_library(gas_core STATIC
  ${GAS_CORE_SOURCES}
)

target_link_libraries(gas_core PRIVATE
  madrona_common
)

target_include_directories(gas_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

if (GAS_USE_DAWN)
  target_link_libraries(gas_core PRIVATE
    dawn::webgpu_dawn
  )

  target_compile_definitions(gas_core PRIVATE
    GAS_SUPPORT_WEBGPU=1
  )
endif()

add_library(gas_gui STATIC
  gui.hpp gui.inl gui.cpp
)

target_link_libraries(gas_gui PRIVATE
  gas_core madrona_common
)

if (GAS_USE_SDL)
  target_link_libraries(gas_gui PRIVATE
    SDL3::SDL3
  )

  target_compile_definitions(gas_gui PRIVATE
    GAS_USE_SDL=1
  )
endif()

add_library(gas_shader_compiler SHARED
  shader_compiler.hpp shader_compiler.inl shader_compiler.cpp
)
target_link_libraries(gas_shader_compiler PRIVATE
  gas_slang
  madrona_hdrs
  madrona_mem
  madrona_noexceptrtti
  madrona_err
)

function(gas_link_hermetic_libcxx tgt)
  if (MADRONA_WINDOWS)
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

gas_link_hermetic_libcxx(gas_shader_compiler)

if (GAS_USE_DAWN)
  target_link_libraries(gas_shader_compiler PRIVATE
    dawn::webgpu_dawn
  )

  target_compile_definitions(gas_shader_compiler PRIVATE
    GAS_SUPPORT_WEBGPU=1
  )

  target_include_directories(gas_shader_compiler SYSTEM PRIVATE
    ${DAWN_BUNDLED_DIR}/include/src/tint
  )
endif()

add_executable(gas_frontend
  frontend.cpp 
)

target_compile_definitions(gas_frontend PRIVATE
  GAS_SRC_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/\"
)

target_link_libraries(gas_frontend
  PRIVATE 
    gas_core
    gas_gui
    madrona_common
)

add_subdirectory(test)
