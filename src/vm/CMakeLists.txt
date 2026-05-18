add_library(vm STATIC
  vm.cpp
  compiler.cpp
)
target_include_directories(vm PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vm PUBLIC lang)
