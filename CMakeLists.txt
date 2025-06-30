project(ChimeraTK-DeviceAccess-OPC-UA-Backend)
cmake_minimum_required(VERSION 2.8.13)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

set(${PROJECT_NAME}_MAJOR_VERSION 01)
set(${PROJECT_NAME}_MINOR_VERSION 05)
set(${PROJECT_NAME}_PATCH_VERSION 01)
include(cmake/set_version_numbers.cmake)

find_package(Boost COMPONENTS unit_test_framework)
find_package(LibXml2 REQUIRED)

find_package(ChimeraTK-DeviceAccess 03.14 REQUIRED)
find_package(open62541 01.04 REQUIRED)

# add support for latest C++ standard to compiler flags
include(cmake/enable_latest_cxx_support.cmake)
include(cmake/enable_code_style_check.cmake)
include_directories(include)
include_directories(SYSTEM ${LIBXML2_INCLUDE_DIR})

aux_source_directory(${CMAKE_SOURCE_DIR}/src library_sources)

add_library(${PROJECT_NAME} SHARED ${library_sources})
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION} SOVERSION ${${PROJECT_NAME}_SOVERSION})
target_link_libraries(${PROJECT_NAME}
      PUBLIC ChimeraTK::ChimeraTK-DeviceAccess
      PRIVATE open62541::open62541
      PRIVATE ${LIBXML2_LIBRARIES})

# --no-as-needed: force linking against this library. This is required for proper registering.
target_link_options(${PROJECT_NAME} PUBLIC "-Wl,--no-as-needed")

# do not remove runtime paths of the library when installing (helps for unusually located implicit dependencies)
set_property(TARGET ${PROJECT_NAME} PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)

if(Boost_UNIT_TEST_FRAMEWORK_FOUND)
  add_subdirectory(test)
else()
      message(STATUS "Boost unit test framework not found. No tests will be build.")
endif()

# Install the library and the executables
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
      EXPORT ${PROJECT_NAME}Targets
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# we support our cmake EXPORTS as imported targets
set(PROVIDES_EXPORTED_TARGETS 1)

# we need the public dependencies so create_cmake_config_files can find them as implicit dependencies
list(APPEND ${PROJECT_NAME}_PUBLIC_DEPENDENCIES "open62541;ChimeraTK-DeviceAccess")
include(${CMAKE_SOURCE_DIR}/cmake/create_cmake_config_files.cmake)
