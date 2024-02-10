cmake_minimum_required(VERSION 3.26)

project(plpaker VERSION 0.12.0.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(FetchContent)

FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        b7cf1246284b49dfe7f1288e6f739b7a3a9d966b
)
FetchContent_MakeAvailable(stb)

FetchContent_Declare(
  zlib
  GIT_REPOSITORY https://github.com/madler/zlib.git
  GIT_TAG        d9243a0f066fd0ad6e7597be620ab8d8bfe5ae72
)
FetchContent_MakeAvailable(zlib)

FetchContent_Declare(
  argh
  GIT_REPOSITORY https://github.com/adishavit/argh.git
  GIT_TAG        431bf323acd3bb805483491ff277dac5d85772ad
)
FetchContent_MakeAvailable(argh)

configure_file(src/version.h.in version.h)

add_library(plpak
  src/paker.cpp src/paker.hpp
)

target_compile_features(plpak
PUBLIC
  cxx_std_23
)

target_include_directories(plpak
PRIVATE
  ${PROJECT_BINARY_DIR}
  ${stb_SOURCE_DIR}
)

target_link_libraries(plpak
PRIVATE
  nlohmann_json::nlohmann_json
  zlibstatic
)

add_executable(${PROJECT_NAME}
  src/main.cpp
  res/app.rc
)

target_include_directories(${PROJECT_NAME}
PRIVATE
  ${argh_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME}
PRIVATE
  plpak
)
