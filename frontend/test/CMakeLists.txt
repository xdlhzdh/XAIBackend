enable_testing()
add_test(
  NAME xai_offload_matmul
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/check_matmul.sh)
set_tests_properties(xai_offload_matmul PROPERTIES
  ENVIRONMENT "XAI_COMPILE=$<TARGET_FILE:xai-compile>;OUT_DIR=${CMAKE_BINARY_DIR}/test-output")
