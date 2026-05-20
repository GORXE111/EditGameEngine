add_executable(farmcode_tests
  test_main.cpp
  test_smoke.cpp
  test_lexer.cpp
  test_parser.cpp
  test_interp.cpp
  test_vm.cpp
  test_sim.cpp
  test_blueprint.cpp
  test_sync.cpp
  test_progress.cpp
  test_enhance.cpp
  test_fleet.cpp
  test_lists.cpp
  test_editor.cpp
)
target_include_directories(farmcode_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(farmcode_tests PRIVATE
  doctest::doctest lang vm sim blueprint)

add_test(NAME farmcode_tests COMMAND farmcode_tests)
