project(DeviceAccess-OPC-UA-Backend)
cmake_minimum_required(VERSION 2.8)

set(${PROJECT_NAME}_MAJOR_VERSION 00)
set(${PROJECT_NAME}_MINOR_VERSION 00)
set(${PROJECT_NAME}_PATCH_VERSION 01)
include(cmake/set_version_numbers.cmake)

# Needed by ControlsystemAdapter-OPC-UA-Adapter in the tests
find_package(LibXml2 REQUIRED)
include_directories(SYSTEM ${LIBXML2_INCLUDE_DIR})

#Moderate version of the compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC -Wextra -ansi -Wuninitialized")

#use -DCMAKE_BUILD_TYPE=Debug in your cmake command to turn on the coverage option
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb")

# linker flags
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-dynamic")

include(cmake/add_dependency.cmake)
add_dependency(mtca4u-deviceaccess 00.29 REQUIRED)
add_dependency(ChimeraTK-ControlSystemAdapter-OPCUAAdapter 01.05 REQUIRED)

# add support for latest C++ standard to compiler flags
include(cmake/enable_latest_cxx_support.cmake)

include_directories( include )

aux_source_directory(${CMAKE_SOURCE_DIR}/src library_sources)

add_library(${PROJECT_NAME} SHARED ${library_sources} )
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION} SOVERSION ${${PROJECT_NAME}_SOVERSION})

FIND_PACKAGE(Boost COMPONENTS unit_test_framework)

if(Boost_UNIT_TEST_FRAMEWORK_FOUND)
  ENABLE_TESTING()
  FILE( COPY ${CMAKE_SOURCE_DIR}/test/test_mapping.xml DESTINATION ${PROJECT_BINARY_DIR})
  add_executable(testClient ${CMAKE_SOURCE_DIR}/test/testClient.C )
  target_link_libraries(testClient  ${mtca4u-deviceaccess_LIBRARIES} 
                                    ${ChimeraTK-ControlSystemAdapter-OPCUAAdapter_LIBRARIES}
                                    ${Boost_LIBRARIES}
                                    ${PROJECT_NAME})
  add_test(testClient testSoftwareMasterBackend)
else()
  message(STATUS "Boost unit test framework not found. No tests will be build.")
endif()

