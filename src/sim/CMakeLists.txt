add_library(sim STATIC
  sim.cpp
  world.cpp
  robot_host.cpp
)
target_include_directories(sim PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(sim PUBLIC lang)
