cmake_minimum_required(VERSION 3.26)

project(plpaker VERSION 0.11.0.0 LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

include(GNUInstallDirs)

find_package(ZLIB)
find_package(stb)
find_package(nlohmann_json)
find_package(argh)

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
)

target_link_libraries(plpak
PRIVATE
  nlohmann_json::nlohmann_json
  ZLIB::ZLIB
  stb::stb
)

add_executable(${PROJECT_NAME}
  src/main.cpp
  res/app.rc
)

target_link_libraries(${PROJECT_NAME}
PRIVATE
  plpak
  argh::argh
)
