list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if (NOT PROJECT_NAME)
  cmake_minimum_required(VERSION 3.26 FATAL_ERROR)
  cmake_policy(VERSION 3.26)
  
  list(APPEND LANGS C CXX)
  if (APPLE)
    list(APPEND LANGS OBJC)
  endif()

  option(GAS_BRT_EXTERNAL_DIR "" "")
  if (NOT GAS_BRT_EXTERNAL_DIR)
    message(FATAL_ERROR "GAS_BRT_EXTERNAL_DIR option must be set for standalone build")
  endif()

  include("${GAS_BRT_EXTERNAL_DIR}/cmake/toolchain.cmake")

  project(gas LANGUAGES ${LANGS})

  include("${GAS_BRT_EXTERNAL_DIR}/cmake/sys-detect.cmake")
  include("${GAS_BRT_EXTERNAL_DIR}/cmake/cxx-setup.cmake")

  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/pdb)

  add_subdirectory("${GAS_BRT_EXTERNAL_DIR}" brt)
endif()

add_subdirectory(deps)

include(gas-find-deps)

list(APPEND GAS_CORE_SOURCES
  gas.hpp gas.inl gas.cpp
  backend_common.hpp 
  mem.hpp mem.cpp
  linux.hpp windows.hpp
)

if (GAS_USE_DAWN OR EMSCRIPTEN)
  list(APPEND GAS_CORE_SOURCES
    wgpu.hpp wgpu.cpp wgpu_init.hpp
  )
endif()

add_library(gas_core STATIC
  ${GAS_CORE_SOURCES}
)

target_link_libraries(gas_core PUBLIC brt)

if (GAS_USE_DAWN)
  target_link_libraries(gas_core PRIVATE gas_dawn)
endif()

add_library(gas_ui STATIC
  gas_ui.hpp gas_ui.inl gas_ui.cpp
)

target_link_libraries(gas_ui PRIVATE
  gas_core brt
)

if (GAS_USE_SDL)
  target_link_libraries(gas_ui PRIVATE gas_sdl)
endif()

add_library(gas_imgui STATIC
  gas_imgui.hpp gas_imgui.cpp
)

target_compile_definitions(gas_imgui PRIVATE 
  GAS_IMGUI_SHADER_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/\"
)

target_link_libraries(gas_imgui
  PUBLIC
    gas_imgui_hdrs
  PRIVATE
    gas_core gas_ui gas_imgui_impl brt
)

if (NOT EMSCRIPTEN)
  add_library(gas_shader_compiler SHARED
    shader_compiler.hpp shader_compiler.inl shader_compiler.cpp
  )
  target_link_libraries(gas_shader_compiler PRIVATE
    gas_slang brt
  )
  
  function(gas_link_hermetic_libcxx tgt)
    FetchContent_GetProperties(MadronaBundledToolchain)
  
    set(HERMETIC_LIBCXX_PATH "${madronabundledtoolchain_SOURCE_DIR}/libcxx-hermetic")
  
    target_compile_options(${tgt} PRIVATE -nostdinc++ -nostdlib++)
    target_link_options(${tgt} PRIVATE -nostdlib++)
    target_include_directories(${tgt} SYSTEM PRIVATE
      $<BUILD_INTERFACE:${HERMETIC_LIBCXX_PATH}/include/c++/v1>)
    target_link_libraries(${tgt} PRIVATE
      ${HERMETIC_LIBCXX_PATH}/lib/libc++-hermetic.a
    )
  endfunction()
  
  if (GAS_USE_DAWN)
    add_library(gas_dawn_tint SHARED
      wgpu_shader_compiler.hpp
      wgpu_shader_compiler.cpp
    )
    target_link_libraries(gas_dawn_tint PRIVATE 
      gas_dawn
      gas_dawn_tint_libs
      brt-hdrs
    )
  
    if (WIN32)
      set_property(TARGET gas_dawn_tint PROPERTY
        MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    elseif(NOT EMPSCRIPTEN)
      gas_link_hermetic_libcxx(gas_dawn_tint)
    endif()
  
    target_link_libraries(gas_shader_compiler PRIVATE
      gas_dawn gas_dawn_tint)
  endif()
endif()

add_subdirectory(examples)
add_subdirectory(test)
