add_library(gas_test_common INTERFACE)

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH GAS_SRC_DIR )

target_include_directories(gas_test_common INTERFACE ${GAS_SRC_DIR})
target_link_libraries(gas_test_common INTERFACE
  gas_core
  madrona_common
)

add_executable(gas_test_utils
  gas_table.cpp
  uuid.cpp
)

target_link_libraries(gas_test_utils PRIVATE
  gas_test_common
  gtest_main
)

add_executable(gas_test_gpu
  test_gpu.hpp
  gpu_tmp_input.cpp
  test_gpu_main.cpp
)

target_compile_definitions(gas_test_gpu PRIVATE
  GAS_TEST_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/\"
)

target_link_libraries(gas_test_gpu PRIVATE
  gtest
  gas_test_common
)

add_executable(gas_test_ui
  swapchain.cpp
)

target_link_libraries(gas_test_ui
  gas_test_common
  gas_ui
  gtest_main
)

include(GoogleTest)
gtest_discover_tests(gas_test_utils)
gtest_discover_tests(gas_test_gpu)
gtest_discover_tests(gas_test_ui)
