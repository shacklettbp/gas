if(EMSCRIPTEN)
  set(GAS_EMSCRIPTEN_SHADER_COMPILER_PATH "" CACHE PATH "Path to the shader compiler")
  if(NOT GAS_EMSCRIPTEN_SHADER_COMPILER_PATH)
    message(FATAL_ERROR "gas_add_shaders: you must specify GAS_EMSCRIPTEN_SHADER_COMPILER_PATH when building with EMSCRIPTEN")
  endif()

  add_executable(gas_shader_compiler_frontend IMPORTED GLOBAL)
  set_property(TARGET gas_shader_compiler_frontend PROPERTY
    IMPORTED_LOCATION ${GAS_EMSCRIPTEN_SHADER_COMPILER_PATH}
  )
else()
  add_library(gas_shader_compiler SHARED
    shader_compiler.hpp shader_compiler.inl shader_compiler.cpp
  )

  target_link_libraries(gas_shader_compiler 
    PUBLIC
      brt
    PRIVATE
      gas_slang
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
    else()
      gas_link_hermetic_libcxx(gas_dawn_tint)
    endif()

    target_link_libraries(gas_shader_compiler PRIVATE
      gas_dawn gas_dawn_tint)
  endif()

  add_executable(gas_shader_compiler_frontend
    shader_compiler_frontend.cpp
  )

  target_link_libraries(gas_shader_compiler_frontend
    PRIVATE gas_shader_compiler
  )
endif()

function(gas_add_shaders)
  set(ONE_VALUE_ARGS TARGET SHADER_ENUM SHADER_CLASS CPP_NAMESPACE)
  set(MULTI_VALUE_ARGS SHADERS ARGS)
  cmake_parse_arguments(A "" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})
  
  if(NOT A_TARGET)
    message(FATAL_ERROR "gas_add_shaders: you must specify TARGET")
  endif()
  if(NOT A_SHADER_ENUM)
    message(FATAL_ERROR "gas_add_shaders: you must specify SHADER_ENUM")
  endif()
  if(NOT A_SHADER_CLASS)
    message(FATAL_ERROR "gas_add_shaders: you must specify SHADER_CLASS")
  endif()
  if(NOT A_CPP_NAMESPACE)
    message(FATAL_ERROR "gas_add_shaders: you must specify CPP_NAMESPACE")
  endif()
  if(NOT A_SHADERS)
    message(FATAL_ERROR "gas_add_shaders: you must specify at least one SHADERS")
  endif()

  list(LENGTH A_SHADERS A_SHADERS_LEN)
  math(EXPR REMAINDER "${A_SHADERS_LEN} % 2")
  if(NOT REMAINDER EQUAL 0)
    message(FATAL_ERROR "gas_add_shaders: SHADERS must contain enum name and source path pairs")
  endif()

  set(SHADER_NAMES "")
  set(SHADER_SOURCES "")
  set(IDX 0)
  while(IDX LESS A_SHADERS_LEN)
    list(GET A_SHADERS ${IDX} SHADER_NAME) 
    math(EXPR IDX "${IDX} + 1")
    list(GET A_SHADERS ${IDX} SHADER_SOURCE)
    math(EXPR IDX "${IDX} + 1")
    list(APPEND SHADER_NAMES ${SHADER_NAME})
    list(APPEND SHADER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE}") 
  endwhile()

  set(SHADER_OBJS "")
  foreach(SHADER_SOURCE ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME_WE)
    set(DEP_FILE "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.shader_d")
    set(OBJ_FILE "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.shader_o")

    add_custom_command(
      OUTPUT "${OBJ_FILE}"
      COMMAND gas_shader_compiler_frontend compile
        "${OBJ_FILE}" "${DEP_FILE}" "${SHADER_SOURCE}" ${A_ARGS}
      DEPENDS "${SHADER_SOURCE}" gas_shader_compiler_frontend
      DEPFILE "${DEP_FILE}"
      BYPRODUCTS "${DEP_FILE}"
      VERBATIM
    )

    list(APPEND SHADER_OBJS "${OBJ_FILE}")
  endforeach()

  set(METADATA_FILE "${CMAKE_CURRENT_BINARY_DIR}/${A_TARGET}.metadata")
  file(WRITE "${METADATA_FILE}" "")
  list(LENGTH SHADER_SOURCES NUM_SHADERS)
  file(APPEND "${METADATA_FILE}" "${NUM_SHADERS}\n")

  foreach(SHADER_NAME ${SHADER_NAMES})
    file(APPEND "${METADATA_FILE}" "${SHADER_NAME}\n")
  endforeach()

  foreach(SHADER_SOURCE ${SHADER_SOURCES})
    file(APPEND "${METADATA_FILE}" "${SHADER_SOURCE}\n")
  endforeach()

  # Write compiled shader object paths
  foreach(OBJ ${SHADER_OBJS})
    file(APPEND "${METADATA_FILE}" "${OBJ}\n")
  endforeach()

  list(LENGTH A_ARGS NUM_ARGS)
  file(APPEND "${METADATA_FILE}" "${NUM_ARGS}\n")
  foreach(ARG ${A_ARGS})
    file(APPEND "${METADATA_FILE}" "${ARG}\n") 
  endforeach()
  
  set(COMPILED_OUTPUT_BASE_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/")
  set(RELOAD_INFO_FILE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders_metadata/${A_TARGET}.reload")

  set(HPP_OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gen/")
  set(HPP_OUT "${HPP_OUT_DIR}/${A_TARGET}.hpp")

  set(COMPILED_OUT_NAME "${A_TARGET}.shader_blob")

  list(APPEND LINK_OUTPUTS
    "${COMPILED_OUTPUT_BASE_DIR}/spirv/${COMPILED_OUT_NAME}"
    "${COMPILED_OUTPUT_BASE_DIR}/wgsl/${COMPILED_OUT_NAME}"
    "${COMPILED_OUTPUT_BASE_DIR}/mtl/${COMPILED_OUT_NAME}"
    "${COMPILED_OUTPUT_BASE_DIR}/dxil/${COMPILED_OUT_NAME}"
    "${RELOAD_INFO_FILE}"
    "${HPP_OUT}"
  )
  add_custom_command(
    OUTPUT ${LINK_OUTPUTS}
    COMMAND gas_shader_compiler_frontend link
      "${A_SHADER_ENUM}" "${A_SHADER_CLASS}" "${A_CPP_NAMESPACE}" "${COMPILED_OUTPUT_BASE_DIR}" "${COMPILED_OUT_NAME}" "${RELOAD_INFO_FILE}" "${HPP_OUT}" "${METADATA_FILE}"
    DEPENDS gas_shader_compiler_frontend ${METADATA_FILE} ${SHADER_OBJS}
    VERBATIM
  )

  add_custom_target(${A_TARGET}-link
    DEPENDS ${LINK_OUTPUTS}
  )

  add_library(${A_TARGET} INTERFACE)
  add_dependencies(${A_TARGET} ${A_TARGET}-link)
  target_include_directories(${A_TARGET} INTERFACE "${HPP_OUT_DIR}")
  target_link_libraries(${A_TARGET} INTERFACE gas_core)
endfunction()