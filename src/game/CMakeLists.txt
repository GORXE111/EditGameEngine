add_executable(game WIN32 main.cpp app.cpp)
target_include_directories(game PRIVATE ${CMAKE_SOURCE_DIR}/src)
# SDL2main provides the WinMain shim that forwards to our main() (WIN32
# subsystem app -> no console window). Must precede SDL2.
target_link_libraries(game PRIVATE
  SDL2::SDL2main lang vm sim blueprint imgui::imgui)

# Copy the SDL2 runtime DLL next to game.exe so it launches from build/bin.
add_custom_command(TARGET game POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:game>
  VERBATIM)
