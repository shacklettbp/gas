add_executable(gas_test_utils
  gas_table.cpp
  uuid.cpp
)

target_link_libraries(gas_test_utils PRIVATE
  gtest_main
  gas_core
  madrona_common
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
  gas_core
  madrona_common
)

add_executable(gas_test_gui
  swapchain.cpp
)

target_link_libraries(gas_test_gui
  gtest_main
  gas_core
  gas_gui
  madrona_common
)

include(GoogleTest)
gtest_discover_tests(gas_test_utils)
gtest_discover_tests(gas_test_gpu)
gtest_discover_tests(gas_test_gui)
