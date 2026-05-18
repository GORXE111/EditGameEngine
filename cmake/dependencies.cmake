# Third-party dependencies via FetchContent.
# SDL2 + doctest have CMake. Dear ImGui + imnodes do not, so we build a
# combined `imgui` static target ourselves (imgui core + SDL2/OpenGL3
# backends + imnodes).

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# ---- SDL2 ----
FetchContent_Declare(
  SDL2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-2.30.9
  GIT_SHALLOW TRUE
)
set(SDL_SHARED ON  CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)

# ---- doctest ----
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11
  GIT_SHALLOW TRUE
)

# ---- Dear ImGui (no CMake) ----
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.5
  GIT_SHALLOW TRUE
)

# ---- imnodes (no CMake) ----
FetchContent_Declare(
  imnodes
  GIT_REPOSITORY https://github.com/Nelarius/imnodes.git
  GIT_TAG v0.5
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SDL2 doctest)
FetchContent_MakeAvailable(imgui imnodes)

find_package(OpenGL REQUIRED)

# Build the combined imgui target.
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
  ${imnodes_SOURCE_DIR}/imnodes.cpp
)
target_include_directories(imgui PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${imgui_SOURCE_DIR}/misc/cpp
  ${imnodes_SOURCE_DIR}
)
# imnodes (and several imgui helpers) require the math operators; must be
# defined before any imgui.h include, so apply it as a compile definition.
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(imgui PUBLIC SDL2::SDL2 OpenGL::GL)
if(MSVC)
  target_compile_options(imgui PRIVATE /W0)
endif()

add_library(imgui::imgui ALIAS imgui)
