add_executable(gas_tests
  gas_table.cpp
  uuid.cpp
)

target_link_libraries(gas_tests
  gtest_main
  gas_core
  madrona_common
)

add_executable(gas_gui_tests
  swapchain.cpp
)

target_link_libraries(gas_gui_tests
  gtest_main
  gas_core
  gas_gui
  madrona_common
)

include(GoogleTest)
gtest_discover_tests(gas_tests)
gtest_discover_tests(gas_gui_tests)
