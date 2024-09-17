add_executable(gas_example_renderer
  example_renderer.cpp 
)

target_compile_definitions(gas_example_renderer PRIVATE
  GAS_EXAMPLES_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/\"
)

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH PARENT_DIR)
cmake_path(GET PARENT_DIR PARENT_PATH PARENT_DIR)

target_include_directories(gas_example_renderer PRIVATE
  ${PARENT_DIR}
)

target_link_libraries(gas_example_renderer
  PRIVATE 
    gas_core
    gas_ui
    gas_imgui
    madrona_common
)

