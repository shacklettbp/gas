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
  gas_input.hpp gas_input.inl gas_input.cpp
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

include(gas-shader-compile)

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

gas_add_shaders(
  TARGET
    gas_imgui_shaders
  SHADER_ENUM
    ImGuiShaderID 
  SHADER_CLASS
    ImGuiShaders
  CPP_NAMESPACE
    gas
  SHADERS
    Render imgui.slang
)

target_link_libraries(gas_imgui PRIVATE gas_imgui_shaders)

add_subdirectory(examples)
add_subdirectory(test)
