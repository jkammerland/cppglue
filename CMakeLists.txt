cmake_minimum_required(VERSION 3.20)
project(cppglue VERSION 0.1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

option(USE_DEV_SETTINGS "Use dev settings" OFF)
if(USE_DEV_SETTINGS)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ccache.cmake)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/use_mold_linker.cmake)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/common_settings.cmake)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/print_compiler_and_flags.cmake)
endif()

# THE LIBRARY
add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# Define public dependencies (if any)
set(CPPGLUE_PUBLIC_DEPENDENCIES "")
# If you had dependencies, you would add them like this: list(APPEND CBOR_TAGS_PUBLIC_DEPENDENCIES "find_dependency(SomePackage)")

include(installation_target)
install_target(${PROJECT_NAME})

# add python bindings: pybind11
option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)
if(BUILD_PYTHON_BINDINGS)
  add_subdirectory(py-gen)
  
  # Install Python module to the Python package directory
  install(TARGETS ${PROJECT_NAME}
          LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}
          COMPONENT python)
          
  # Install package files
  install(FILES 
          ${CMAKE_CURRENT_BINARY_DIR}/setup.py
          ${CMAKE_CURRENT_BINARY_DIR}/pyproject.toml
          ${CMAKE_CURRENT_BINARY_DIR}/README.md
          DESTINATION ${CMAKE_INSTALL_PREFIX}
          COMPONENT python)
          
  # Create empty __init__.py
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/__init__.py "")
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/__init__.py
          DESTINATION ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}
          COMPONENT python)
endif()
