get_property(MULTI_CFG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (MULTI_CFG)
    set(DEP_LIB_OUT_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/$<CONFIG>")
else ()
    set(DEP_LIB_OUT_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif ()

function(find_sdl)
  if (SDL_BUNDLED_DIR)
    find_package(SDL3 CONFIG
      PATHS "${SDL_BUNDLED_DIR}"
      NO_DEFAULT_PATH
      REQUIRED
    )
  else()
    find_package(SDL3 CONFIG REQUIRED)
  endif()

  get_target_property(SDL_IMPLIB SDL3::SDL3-shared IMPORTED_IMPLIB_RELEASE)
  get_target_property(SDL_IN_LOC SDL3::SDL3-shared IMPORTED_LOCATION_RELEASE)

  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    get_target_property(SDL_LIB_NAME SDL3::SDL3-shared IMPORTED_SONAME_RELEASE)
  else ()
    cmake_path(GET SDL_IN_LOC FILENAME SDL_LIB_NAME)
  endif ()

  set(SDL_OUT_LOC "${DEP_LIB_OUT_DIR}/${SDL_LIB_NAME}")
  
  add_custom_command(
    OUTPUT ${SDL_OUT_LOC}
    DEPENDS ${SDL_IN_LOC}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SDL_IN_LOC}  ${SDL_OUT_LOC}
  )

  add_custom_target(gas_sdl_shlib
    DEPENDS ${SDL_OUT_LOC}
  )
  
  add_library(gas_sdl SHARED IMPORTED GLOBAL)
  add_dependencies(gas_sdl gas_sdl_shlib)
  set_target_properties(gas_sdl PROPERTIES IMPORTED_LOCATION
    ${SDL_OUT_LOC}
  )

  if (WIN32)
    set_target_properties(gas_sdl PROPERTIES IMPORTED_IMPLIB
      ${SDL_IMPLIB}
    )
  endif()

  target_link_libraries(gas_sdl INTERFACE SDL3::Headers)

  target_compile_definitions(gas_sdl INTERFACE
    GAS_USE_SDL=1
  )
endfunction()

if (GAS_USE_SDL)
  find_sdl()
endif()
unset(find_sdl)

function(find_dawn)
  find_package(Dawn REQUIRED CONFIG
    PATHS "${DAWN_BUNDLED_DIR}"
    NO_DEFAULT_PATH
    REQUIRED
  )

  add_library(gas_dawn INTERFACE)
  target_link_libraries(gas_dawn INTERFACE dawn::dawn_public_config)
  get_target_property(DAWN_COMPILE_DEFS 
      dawn::webgpu_dawn INTERFACE_COMPILE_DEFINITIONS)
  target_compile_definitions(gas_dawn INTERFACE
    GAS_SUPPORT_WEBGPU=1
    ${DAWN_COMPILE_DEFS}
  )

  get_target_property(DAWN_IMPLIB dawn::webgpu_dawn IMPORTED_IMPLIB_RELEASE)
  get_target_property(DAWN_IN_LOC dawn::webgpu_dawn IMPORTED_LOCATION_RELEASE)

  cmake_path(GET DAWN_IN_LOC FILENAME DAWN_LIB_NAME)
  set(DAWN_OUT_LOC "${DEP_LIB_OUT_DIR}/${DAWN_LIB_NAME}")

  add_custom_command(
    OUTPUT ${DAWN_OUT_LOC}
    DEPENDS ${DAWN_IN_LOC}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DAWN_IN_LOC}  ${DAWN_OUT_LOC}
  )

  add_custom_target(gas_dawn_shlib_copy
    DEPENDS ${DAWN_OUT_LOC}
  )
  
  add_library(gas_dawn_shlib SHARED IMPORTED GLOBAL)
  add_dependencies(gas_dawn_shlib gas_dawn_shlib_copy)
  set_target_properties(gas_dawn_shlib PROPERTIES IMPORTED_LOCATION
    ${DAWN_OUT_LOC}
  )
  
  target_link_libraries(gas_dawn INTERFACE gas_dawn_shlib)

  if (WIN32)
    set_target_properties(gas_dawn_shlib PROPERTIES IMPORTED_IMPLIB
      ${DAWN_IMPLIB}
    )
  endif()

  add_library(gas_dawn_tint_libs INTERFACE)
  target_include_directories(gas_dawn_tint_libs SYSTEM INTERFACE
    ${DAWN_BUNDLED_DIR}/include/src/tint
  )

  if (WIN32)
    cmake_path(GET DAWN_IMPLIB PARENT_PATH DAWN_IMPLIB_DIR)

    file(GLOB TINT_LIBS "${DAWN_IMPLIB_DIR}/tint*.lib")

    target_link_libraries(gas_dawn_tint_libs INTERFACE
      ${TINT_LIBS}
      ${DAWN_IMPLIB_DIR}/SPIRV-Tools-opt.lib
      ${DAWN_IMPLIB_DIR}/SPIRV-Tools.lib
    )
  else()
    cmake_path(GET DAWN_IN_LOC PARENT_PATH DAWN_LIB_DIR)

    file(GLOB TINT_LIBS "${DAWN_LIB_DIR}/libtint*.a")

    target_link_libraries(gas_dawn_tint_libs INTERFACE
      ${TINT_LIBS}
      ${DAWN_LIB_DIR}/libSPIRV-Tools-opt.a
      ${DAWN_LIB_DIR}/libSPIRV-Tools.a
    )
  endif()
endfunction()

if (GAS_USE_DAWN)
  find_dawn()
endif()
unset(find_dawn)

function(find_slang)
  if (WIN32)
    find_file(SLANG_IN_LOC slang.dll
      PATHS "${SLANG_BUNDLED_DIR}/bin"
      REQUIRED
      NO_DEFAULT_PATH
    )

    find_file(SLANG_GLSLANG_IN_LOC slang-glslang.dll
      PATHS "${SLANG_BUNDLED_DIR}/bin"
      REQUIRED
      NO_DEFAULT_PATH
    )

    find_library(SLANG_IMPLIB_LOC
      NAMES slang.lib
      PATHS "${SLANG_BUNDLED_DIR}/lib"
      REQUIRED
      NO_DEFAULT_PATH
    )
  else ()
    find_library(SLANG_IN_LOC
      NAMES libslang.so libslang.dylib
      PATHS "${SLANG_BUNDLED_DIR}/lib"
      REQUIRED
      NO_DEFAULT_PATH
    )

    find_library(SLANG_GLSLANG_IN_LOC
      NAMES libslang-glslang.so libslang-glslang.dylib
      PATHS "${SLANG_BUNDLED_DIR}/lib"
      REQUIRED
      NO_DEFAULT_PATH
    )
  endif()
      
  cmake_path(GET SLANG_IN_LOC FILENAME SLANG_LIB_NAME)
  cmake_path(GET SLANG_GLSLANG_IN_LOC FILENAME SLANG_GLSLANG_LIB_NAME)

  set(SLANG_OUT_LOC "${DEP_LIB_OUT_DIR}/${SLANG_LIB_NAME}")
  set(SLANG_GLSLANG_OUT_LOC "${DEP_LIB_OUT_DIR}/${SLANG_GLSLANG_LIB_NAME}")
  
  add_custom_command(
    OUTPUT ${SLANG_OUT_LOC}
    DEPENDS ${SLANG_IN_LOC}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SLANG_IN_LOC}  ${SLANG_OUT_LOC}
  )

  add_custom_command(
    OUTPUT ${SLANG_GLSLANG_OUT_LOC}
    DEPENDS ${SLANG_GLSLANG_IN_LOC}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${SLANG_GLSLANG_IN_LOC}  ${SLANG_GLSLANG_OUT_LOC}
  )
  
  add_custom_target(gas_slang_shlib
    DEPENDS ${SLANG_OUT_LOC} ${SLANG_GLSLANG_OUT_LOC}
  )
  
  add_library(gas_slang SHARED IMPORTED GLOBAL)
  add_dependencies(gas_slang gas_slang_shlib)
  set_target_properties(gas_slang PROPERTIES IMPORTED_LOCATION
    ${SLANG_OUT_LOC}
  )

  if (WIN32)
    set_target_properties(gas_slang PROPERTIES IMPORTED_IMPLIB
      ${SLANG_IMPLIB_LOC}
    )
  endif()

  target_include_directories(gas_slang SYSTEM INTERFACE
    $<BUILD_INTERFACE:${SLANG_BUNDLED_DIR}/include>
  )
endfunction()

find_slang()
unset(find_slang)

function(find_imgui)
  add_library(gas_imgui_hdrs INTERFACE)
  target_include_directories(gas_imgui_hdrs SYSTEM INTERFACE
    $<BUILD_INTERFACE:${IMGUI_BUNDLED_DIR}>
  )

  add_library(gas_imgui_impl OBJECT
    ${IMGUI_BUNDLED_DIR}/imgui.cpp
    ${IMGUI_BUNDLED_DIR}/imgui_draw.cpp
    ${IMGUI_BUNDLED_DIR}/imgui_tables.cpp
    ${IMGUI_BUNDLED_DIR}/imgui_widgets.cpp
  )
endfunction()

find_imgui()
unset(find_imgui)
