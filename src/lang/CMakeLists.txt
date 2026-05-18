add_library(lang STATIC
  lang.cpp
  lexer.cpp
  parser.cpp
  printer.cpp
  interpreter.cpp
)
target_include_directories(lang PUBLIC ${CMAKE_SOURCE_DIR}/src)
