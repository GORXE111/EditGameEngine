add_library(blueprint STATIC
  blueprint.cpp
  codec.cpp
)
target_include_directories(blueprint PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(blueprint PUBLIC lang)
