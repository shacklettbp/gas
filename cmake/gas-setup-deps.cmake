get_property(MULTI_CFG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (MULTI_CFG)
    set(DEP_LIB_OUT_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/$<CONFIG>")
else ()
    set(DEP_LIB_OUT_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif ()

if (GAS_USE_SDL)
  if (SDL_BUNDLED_DIR)
    find_package(SDL3 CONFIG
      PATHS "${SDL_BUNDLED_DIR}"
      NO_DEFAULT_PATH
      REQUIRED
    )
  else()
    find_package(SDL3 CONFIG REQUIRED)
  endif()
endif()

if (GAS_USE_DAWN)
  find_package(Dawn REQUIRED CONFIG
    PATHS "${DAWN_BUNDLED_DIR}"
    NO_DEFAULT_PATH
    REQUIRED
  )
endif()

function(add_slang_target)
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
add_slang_target()
