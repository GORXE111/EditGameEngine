add_library(sim STATIC
  sim.cpp
  world.cpp
  robot_host.cpp
  progress.cpp
  fleet.cpp
)
target_include_directories(sim PUBLIC ${CMAKE_SOURCE_DIR}/src)
# fleet.cpp runs N VM instances over one Chunk -> sim depends on vm.
target_link_libraries(sim PUBLIC lang vm)
