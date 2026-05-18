add_executable(farmcode_tests test_main.cpp)
target_include_directories(farmcode_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(farmcode_tests PRIVATE
  doctest::doctest lang vm sim blueprint)

add_test(NAME farmcode_tests COMMAND farmcode_tests)
