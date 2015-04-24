project(KMU)
cmake_minimum_required(VERSION 2.6)

set(VERSION_MAJOR 1)
set(VERSION_MINOR 1)
set(VERSION_PATCH 0)

option (PYTHON
  "Use kmu of python version"
  ON)

option (BUILD_CKMU
  "Build KMU of c version too"
  OFF)

set(CMAKE_INSTALL_PREFIX "/usr")

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_distclean.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_distclean.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

add_custom_target(distclean
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_distclean.cmake)


add_subdirectory(src)
