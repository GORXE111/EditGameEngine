add_library(sim STATIC sim.cpp)
target_include_directories(sim PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(sim PUBLIC vm)
