cmake_minimum_required(VERSION 3.22)

project(py-gen VERSION 0.1.0)

# set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# message(STATUS "CMAKE_POSITION_INDEPENDENT_CODE: ${CMAKE_POSITION_INDEPENDENT_CODE}")

include(get_cpm)

# Remember old source cache
set(CPM_SOURCE_CACHE_OLD ${CPM_SOURCE_CACHE})

set(CPM_SOURCE_CACHE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}/cpm_cache)
cpmaddpackage("gh:fmtlib/fmt#11.1.2")
cpmaddpackage("gh:jarro2783/cxxopts@3.2.0")
cpmaddpackage("gh:marzer/tomlplusplus@3.4.0")
find_package(tomlplusplus REQUIRED)

# Restore old source cache
set(CPM_SOURCE_CACHE ${CPM_SOURCE_CACHE_OLD})

# Find LLVM and Clang packages
find_package(LLVM REQUIRED CONFIG PATHS)
find_package(Clang REQUIRED CONFIG PATHS)
# find_package(LibArchive REQUIRED)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Found Clang ${CLANG_PACKAGE_VERSION}")
message(STATUS "Using ClangConfig.cmake in: ${Clang_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
add_definitions(${LLVM_DEFINITIONS})

# Add executable
add_executable(${PROJECT_NAME})

# Glob all source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Platform-specific configuration
if(WIN32)
  set(CLANG_LIBS clangTooling clangFrontend clangASTMatchers clangBasic clangAST clangSerialization)
else()
  set(CLANG_LIBS
      clangTooling
      clangFrontend
      clangASTMatchers
      clang-cpp
      clangBasic
      clangAST
      clangSerialization)
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt ${CLANG_LIBS} $<$<NOT:$<PLATFORM_ID:Windows>>:tinfo> cppglue cxxopts tomlplusplus::tomlplusplus)
# List all private dependencies of project_name
message(STATUS "${PROJECT_NAME} private dependencies: cppglue cxxopts tomlplusplus ${CLANG_LIBS} $<$<NOT:$<PLATFORM_ID:Windows>>:tinfo>")

# Add the install target
include(installation_target)
install_target(${PROJECT_NAME})
set(CPPGLUE_PUBLIC_DEPENDENCIES "find_dependency(fmt) find_dependency(cppglue) find_dependency(cxxopts)")

add_subdirectory(tests)

# Copy templates to build directory for development/testing
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/templates
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/src/templates
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/templates)

# Install templates alongside executable
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/templates/
        DESTINATION ${CMAKE_INSTALL_BINDIR}/templates
        FILES_MATCHING PATTERN "*.template")

# Copy templates POST_BUILD for development/testing
message(STATUS "Copying templates to build directory")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/src/templates $<TARGET_FILE_DIR:${PROJECT_NAME}>/templates)
