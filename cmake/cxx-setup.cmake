set(GAS_CXX_CLANG FALSE)
set(GAS_CXX_GCC FALSE)
set(GAS_CXX_MSVC FALSE)

if (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  set(GAS_CXX_CLANG TRUE)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(GAS_CXX_GCC TRUE)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(GAS_CXX_MSVC TRUE)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if (CMAKE_CXX_COMPILER_GAS_CXX_VARIANT STREQUAL "MSVC")
    set(GAS_CXX_MSVC TRUE)
    set(GAS_CXX_CLANG_CL TRUE)
  else ()
    set(GAS_CXX_CLANG TRUE)
  endif ()
endif ()

if (GAS_CXX_MSVC)
  string(REPLACE "/DNDEBUG" "" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  string(REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
else()
  string(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -ggdb3")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb3")
endif()

add_library(rdb-sys-defns INTERFACE)

if (GAS_CXX_CLANG)
  list(APPEND GAS_SYS_DEFNS "GAS_CXX_CLANG=1")
endif()

if (GAS_CXX_GCC)
  list(APPEND GAS_SYS_DEFNS "GAS_CXX_GCC=1")
endif()

if (GAS_CXX_MSVC)
  list(APPEND GAS_SYS_DEFNS "GAS_CXX_MSVC=1")
endif()

if (GAS_CXX_CLANG_CL)
  list(APPEND GAS_SYS_DEFNS "GAS_CXX_CLANG_CL=1")
endif()

if (GAS_OS_LINUX)
  list(APPEND GAS_SYS_DEFNS "GAS_OS_LINUX=1")
endif()

if (GAS_OS_MACOS)
  list(APPEND GAS_SYS_DEFNS "GAS_OS_MACOS=1")
endif()

if (GAS_OS_WINDOWS)
  list(APPEND GAS_SYS_DEFNS "GAS_OS_WINDOWS=1")
endif()

if (GAS_ARCH_X64)
  list(APPEND GAS_SYS_DEFNS "GAS_OS_X64=1")
endif()

if (GAS_ARCH_ARM)
  list(APPEND GAS_SYS_DEFNS "GAS_ARCH_ARM=1")
endif()

target_compile_definitions(rdb-sys-defns INTERFACE ${GAS_SYS_DEFNS})

add_library(rdb-cxx-flags INTERFACE)
target_link_libraries(rdb-cxx-flags INTERFACE
    rdb-sys-defns
)

if (GAS_CXX_CLANG OR GAS_CXX_GCC)
  target_compile_options(rdb-cxx-flags INTERFACE
    -pedantic -Wall -Wextra
  )

  if (GAS_ARCH_X64 AND GAS_OS_LINUX)
    target_compile_options(rdb-cxx-flags INTERFACE
      -march=x86-64-v3
    )
  elseif (GAS_ARCH_ARM AND GAS_OS_MACOS)
    target_compile_options(rdb-cxx-flags INTERFACE
      -mcpu=apple-m1
    )
  endif()
elseif (GAS_CXX_MSVC)
  # FIXME: some of these options (/permissive-, /Zc:__cplusplus,
  # /Zc:preprocessor) should just be applied globally to the toolchain
  target_compile_options(rdb-cxx-flags INTERFACE
    /Zc:__cplusplus
    /permissive-
    /W4
    /wd4324 # Struct padded for alignas ... yeah that's the point
    /wd4701 # Potentially uninitialized variable. MSVC analysis really sucks on this
    /wd4244 /wd4267 # Should reenable these
  )

  if (NOT GAS_CXX_CLANG_CL)
    target_compile_options(rdb-cxx-flags INTERFACE
      /Zc:preprocessor
    )
  endif()
endif()

if (GAS_CXX_GCC)
  target_compile_options(rdb-cxx-flags INTERFACE
    -fdiagnostics-color=always  
  )
elseif (GAS_CXX_CLANG)
  target_compile_options(rdb-cxx-flags INTERFACE
    -fcolor-diagnostics
  )
endif ()

if (GAS_CXX_CLANG)
  target_compile_options(rdb-cxx-flags INTERFACE
    -Wshadow
  )
endif()

add_library(rdb-cxx-noexceptrtti INTERFACE)
if (GAS_CXX_GCC OR GAS_CXX_CLANG)
  target_compile_options(rdb-cxx-noexceptrtti INTERFACE
    -fno-exceptions -fno-rtti
  )
elseif (GAS_CXX_MSVC)
  target_compile_options(rdb-cxx-noexceptrtti INTERFACE
    /GR-
  )
else()
  message(FATAL_ERROR "Unsupported compiler frontend")
endif()
