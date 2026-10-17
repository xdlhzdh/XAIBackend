add_executable(xai-compile main.cpp)
target_link_libraries(xai-compile PRIVATE XAIFrontend)
target_compile_definitions(xai-compile PRIVATE
  XAI_LLC_DEFAULT="${XAI_LLC_DEFAULT}")
set_target_properties(xai-compile PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
