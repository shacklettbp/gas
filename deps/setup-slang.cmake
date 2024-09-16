set(SLANG_BUNDLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bundled-slang")

set(SLANG_SRC_DIR "${GAS_BUNDLE_TMP_DIR}/slang")
set(SLANG_BUILD_TIMESTAMP_FILE "${GAS_BUNDLE_TMP_DIR}/slang-build-stamp")
set(SLANG_BUILD_CONFIG_HASH_FILE "${GAS_BUNDLE_TMP_DIR}/slang-build-config-hash")

function(fetch_build_slang)
  FetchContent_Populate(slang-bundled
    GIT_REPOSITORY https://github.com/shader-slang/slang
    GIT_TAG v2024.11
    GIT_PROGRESS ON
    SOURCE_DIR "${SLANG_SRC_DIR}"
  )

  if (NOT WIN32) #FIX
    FetchContent_GetProperties(MadronaBundledToolchain)

    set(HERMETIC_LIBCXX_PATH "${madronabundledtoolchain_SOURCE_DIR}/libcxx-hermetic")
    set(OP_NEWDEL_PATH "${madronabundledtoolchain_SOURCE_DIR}/toolchain-standalone-op-newdel/lib/libmadrona_toolchain_standalone_op_newdel.a")

    set(HERMETIC_LIBCXX_INC_FLAGS "-nostdinc++ -isystem ${HERMETIC_LIBCXX_PATH}/include/c++/v1")
    set(HERMETIC_LIBCXX_LINKER_FLAGS
        "-nostdlib++ ${HERMETIC_LIBCXX_PATH}/lib/libc++-hermetic.a ${OP_NEWDEL_PATH}")
else ()
    set(HERMETIC_LIBCXX_INC_FLAGS "")
    set(HERMETIC_LIBCXX_LINKER_FLAGS "")
endif()

  set(SLANG_CXX_FLAGS "${HERMETIC_LIBCXX_INC_FLAGS}")

  set(SLANG_CMAKE_ARGS)

  list(APPEND SLANG_CMAKE_ARGS
    "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DCMAKE_OBJC_COMPILER=${CMAKE_OBJC_COMPILER}"
    "-DCMAKE_INSTALL_PREFIX=${SLANG_BUNDLED_DIR}"
    "-DCMAKE_CXX_FLAGS=${SLANG_CXX_FLAGS} -DSLANG_ENABLE_DXIL_SUPPORT=1"
    "-DCMAKE_EXE_LINKER_FLAGS=${HERMETIC_LIBCXX_LINKER_FLAGS}"
    "-DCMAKE_SHARED_LINKER_FLAGS=${HERMETIC_LIBCXX_LINKER_FLAGS}"
    "-DCMAKE_MODULE_LINKER_FLAGS=${HERMETIC_LIBCXX_LINKER_FLAGS}"
    "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}"
    "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
    "-DSLANG_ENABLE_GFX=FALSE"
    "-DSLANG_ENABLE_SLANGC=FALSE"
    "-DSLANG_ENABLE_SLANGRT=FALSE"
    "-DSLANG_ENABLE_SLANG_GLSLANG=ON" # This is needed to get spirv-opt compiled which is used for direct SPIRV output
    "-DSLANG_SLANG_LLVM_FLAVOR=DISABLE"
    "-DSLANG_LIB_TYPE=SHARED"
  )

  function(build_slang)
    find_package(Git REQUIRED)

    set(PATCH_STR
[=[diff --git a/source/core/slang-performance-profiler.cpp b/source/core/slang-performance-profiler.cpp
index f08b4998..149c103a 100644
--- a/source/core/slang-performance-profiler.cpp
+++ b/source/core/slang-performance-profiler.cpp
@@ -38,7 +38,7 @@ namespace Slang
                 snprintf(buffer, sizeof(buffer), "[*] %30s", func.key);
                 out << buffer << " \t";
                 auto milliseconds = std::chrono::duration_cast< std::chrono::milliseconds >(func.value.duration);
-                out << func.value.invocationCount << " \t" << milliseconds.count() << "ms\n";
+                out << func.value.invocationCount << " \t" << (uint64_t)milliseconds.count() << "ms\n";
             }
         }
         virtual void clear() override
diff --git a/source/slang/slang-emit.cpp b/source/slang/slang-emit.cpp
index ed9e9046..d6e6728f 100644
--- a/source/slang/slang-emit.cpp
+++ b/source/slang/slang-emit.cpp
@@ -806,7 +806,7 @@ Result linkAndOptimizeIR(
         // For some targets, we are more restrictive about what types are allowed
         // to be used as shader parameters in ConstantBuffer/ParameterBlock.
         // We will check for these restrictions here.
-        checkForInvalidShaderParameterType(targetRequest, irModule, sink);
+        //checkForInvalidShaderParameterType(targetRequest, irModule, sink);
     }
 
     if (sink->getErrorCount() != 0)
]=])

    file(CONFIGURE OUTPUT "${GAS_BUNDLE_TMP_DIR}/slang-patch" CONTENT "${PATCH_STR}" NEWLINE_STYLE UNIX)

    execute_process(COMMAND
      ${GIT_EXECUTABLE} checkout 
          "source/core/slang-performance-profiler.cpp"
          "source/slang/slang-emit.cpp"
      WORKING_DIRECTORY "${SLANG_SRC_DIR}"
      COMMAND_ERROR_IS_FATAL ANY
    )

    execute_process(COMMAND
      ${GIT_EXECUTABLE} apply "${GAS_BUNDLE_TMP_DIR}/slang-patch"
      WORKING_DIRECTORY "${SLANG_SRC_DIR}"
      COMMAND_ERROR_IS_FATAL ANY
    )

    execute_process(COMMAND ${CMAKE_COMMAND}
      --preset default
      ${SLANG_CMAKE_ARGS}
      WORKING_DIRECTORY ${SLANG_SRC_DIR}
      COMMAND_ERROR_IS_FATAL ANY
    )

    execute_process(COMMAND ${CMAKE_COMMAND}
      --build --preset release
      WORKING_DIRECTORY ${SLANG_SRC_DIR} 
      COMMAND_ERROR_IS_FATAL ANY
    )

    execute_process(COMMAND ${CMAKE_COMMAND}
      --install ${SLANG_SRC_DIR}/build --config Release
      COMMAND_ERROR_IS_FATAL ANY
    )

    file(SHA512 "${CMAKE_CURRENT_LIST_FILE}" SLANG_CONFIG_FILE_HASH)

    file(TOUCH "${SLANG_BUILD_TIMESTAMP_FILE}")
    file(WRITE "${SLANG_BUILD_CONFIG_HASH_FILE}" "${SLANG_CONFIG_FILE_HASH}")
  endfunction()

  build_slang()
endfunction()

function(check_build_slang)
  file(SHA512 "${CMAKE_CURRENT_LIST_FILE}" SLANG_CONFIG_FILE_HASH)

  if (EXISTS "${SLANG_BUILD_TIMESTAMP_FILE}")
    file(READ "${SLANG_BUILD_TIMESTAMP_FILE}" CUR_BUILD_TIMESTAMP)
  else()
    set(CUR_BUILD_TIMESTAMP "")
  endif()

  if (EXISTS "${SLANG_BUILD_CONFIG_HASH_FILE}")
    file(READ "${SLANG_BUILD_CONFIG_HASH_FILE}" CUR_BUILD_CONFIG_HASH)
  else()
    set(CUR_BUILD_CONFIG_HASH "")
  endif()

  set(NEED_BUILD_SLANG FALSE)
  if (NOT "${CUR_BUILD_CONFIG_HASH}" MATCHES "${SLANG_BUILD_CONFIG_FILE_HASH}")
    set(NEED_BUILD_SLANG TRUE)
  endif()

  if (NOT EXISTS "${SLANG_BUILD_TIMESTAMP_FILE}")
    set(NEED_BUILD_SLANG TRUE)
  else()
    if ("${CMAKE_CURRENT_LIST_FILE}" IS_NEWER_THAN "${SLANG_BUILD_TIMESTAMP_FILE}")
      set(NEED_BUILD_SLANG TRUE)
    endif()
  endif()

  if (NEED_BUILD_SLANG)
    fetch_build_slang()
  endif()
endfunction()

check_build_slang()
