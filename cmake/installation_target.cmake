# This function sets up the installation rules for a given target in a CMake project.
#
# Usage:
#   install_target(target_name)
#
# Parameters:
#   target_name - The name of the target to install.
#
# What it does:
# - Includes necessary CMake modules for installation directories and package configuration helpers.
# - Installs the target's binaries (library, archive, runtime) to the appropriate directories.
# - Installs the target's header files from the 'include' directory.
# - Generates and installs an export file for the target.
# - Generates a version file for the package and installs it.
# - Configures and generates a package configuration file based on a template.
# - Installs the generated configuration and version files.
#
# This function helps in setting up the installation process for a CMake target, ensuring that
# all necessary files are installed to the correct locations and that the package can be found
# and used by other projects.
function(install_target target_name)
  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)

  # Installation settings
  install(
    TARGETS ${target_name}
    EXPORT ${target_name}-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  # Install headers
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  # Generate and install export file
  install(
    EXPORT ${target_name}-targets
    FILE ${target_name}-targets.cmake
    NAMESPACE ${target_name}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${target_name})

  # Generate the version file
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${target_name}-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

  # Configure the dependencies string for config file
  string(JOIN "\n" ${target_name}_PUBLIC_DEPENDENCIES ${${target_name}_PUBLIC_DEPENDENCIES})

  # Generate the config file
  configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/${target_name}-config.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/${target_name}-config.cmake"
                                INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${target_name})

  # Install config files
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${target_name}-config.cmake" "${CMAKE_CURRENT_BINARY_DIR}/${target_name}-config-version.cmake" DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${target_name})
endfunction()
