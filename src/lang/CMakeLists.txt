add_library(lang STATIC lang.cpp)
target_include_directories(lang PUBLIC ${CMAKE_SOURCE_DIR}/src)
