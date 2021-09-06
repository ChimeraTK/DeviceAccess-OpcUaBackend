project(ChimeraTK-DeviceAccess-OPC-UA-Backend)
cmake_minimum_required(VERSION 2.8.13)

set(${PROJECT_NAME}_MAJOR_VERSION 00)
set(${PROJECT_NAME}_MINOR_VERSION 05)
set(${PROJECT_NAME}_PATCH_VERSION 07)
include(cmake/set_version_numbers.cmake)

include(cmake/set_default_build_to_release.cmake)
include(cmake/set_default_flags.cmake)

# Options
option(WithCSA-OPCUA "Test against ControlsystemAdapter-OPC-UA")


include(cmake/add_dependency.cmake)
add_dependency(ChimeraTK-DeviceAccess 02.06 REQUIRED)
add_dependency(open62541 00.01 REQUIRED)

if(WithCSA-OPCUA)
  add_dependency(ChimeraTK-ControlSystemAdapter-OPCUAAdapter 01.05 REQUIRED)
  # Needed by ControlsystemAdapter-OPC-UA-Adapter in the tests
  find_package(LibXml2 REQUIRED)
  include_directories(SYSTEM ${LIBXML2_INCLUDE_DIR})
endif(WithCSA-OPCUA)

# add support for latest C++ standard to compiler flags
include(cmake/enable_latest_cxx_support.cmake)
include_directories( include ${open62541_INCLUDE_DIRS})

aux_source_directory(${CMAKE_SOURCE_DIR}/src library_sources)

add_library(${PROJECT_NAME} SHARED ${library_sources} )
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION} SOVERSION ${${PROJECT_NAME}_SOVERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_FLAGS "-Wl,--enable-new-dtags")
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "${ChimeraTK-DeviceAccess_CXX_FLAGS}")
target_link_libraries(${PROJECT_NAME} ${ChimeraTK-DeviceAccess_LIBRARIES} ${ChimeraTK-ControlSystemAdapter_LIBRARIES} ${open62541_LIBRARIES}) 

find_package( Threads )
FIND_PACKAGE(Boost COMPONENTS unit_test_framework system)


if(Boost_UNIT_TEST_FRAMEWORK_FOUND)
  ENABLE_TESTING()
  if(WithCSA-OPCUA)
  FILE( COPY ${CMAKE_SOURCE_DIR}/test/test_mapping.xml DESTINATION ${PROJECT_BINARY_DIR})
  add_executable(testClient ${CMAKE_SOURCE_DIR}/test/testClient.C )
  target_link_libraries(testClient  ${ChimeraTK-DeviceAccess_LIBRARIES}
                                    ${ChimeraTK-ControlSystemAdapter-OPCUAAdapter_LIBRARIES}
                                    ${open62541_LIBRARIES}
                                    ${Boost_LIBRARIES}
                                    ${PROJECT_NAME})
  add_test(testClient testSoftwareMasterBackend)
  endif(WithCSA-OPCUA)

  #set_property(SOURCE ${CMAKE_SOURCE_DIR}/test/testSimpleServer.C PROPERTY COMPILE_FLAGS "-std=c99")
  #SET_SOURCE_FILES_PROPERTIES( ${CMAKE_SOURCE_DIR}/test/testSimpleServer.C  PROPERTIES LANGUAGE C )  
  add_executable(testSimpleServer ${CMAKE_SOURCE_DIR}/test/testSimpleServer.C )
  target_link_libraries( testSimpleServer ${open62541_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})
  add_test(testClient testSimpleServer)
  
  add_executable(testBrowse ${CMAKE_SOURCE_DIR}/test/testBrowse.C )
  target_link_libraries( testBrowse ${open62541_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})
  add_test(testBrowse testBrowse)
  
  add_executable(testSubscription ${CMAKE_SOURCE_DIR}/test/testSubscription.C )
  target_link_libraries( testSubscription ${open62541_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})
  
  add_executable(testUnifiedBackendTest ${CMAKE_SOURCE_DIR}/test/DummyServer/DummyServer.cc ${CMAKE_SOURCE_DIR}/test/testUnifiedBackendTest.C )
  # force linking against the backend library
  set_target_properties(testUnifiedBackendTest PROPERTIES LINK_FLAGS "-Wl,--no-as-needed")
  target_link_libraries( testUnifiedBackendTest ${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES} ${ChimeraTK-DeviceAccess_LIBRARIES} ${open62541_LIBRARIES})
  target_include_directories(testUnifiedBackendTest PRIVATE ${CMAKE_SOURCE_DIR}/test/DummyServer)
  add_test(testUnifiedBackendTest testUnifiedBackendTest)
#  add_executable(simpleRead ${CMAKE_SOURCE_DIR}/test/simpleRead.C )
#  target_link_libraries( simpleRead ${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})
else()
  message(STATUS "Boost unit test framework not found. No tests will be build.")
endif()

#Install the library and the executables
install(TARGETS ${PROJECT_NAME}
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
  )
  
#all include files go into include/PROJECT_NAME
#The exclusion of ${PROJECT_NAME} prevents the recursive installation of the files just being installed.
#The original headers are in include/*.h, the installed ones in include/PROJECT_NAME/*.h.
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include/${PROJECT_NAME}
          FILES_MATCHING PATTERN "*.h"
          PATTERN ".svn" EXCLUDE
          PATTERN "${PROJECT_NAME}" EXCLUDE
          )

#Create the config files by filling the correct variables into the template (*.cmake.in).
#All variables @VARIABLE@ are replaced with the current values, but they have to be set first....
#Only the reference header is needed. The other variables are for dependencies of this library,
#but there are none.
set(${PROJECT_NAME}_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/include ${Boost_INCLUDE_DIRS}")
set(${PROJECT_NAME}_LIBRARIES "${PROJECT_NAME} ${ChimeraTK-DeviceAccess_LIBRARIES}")
set(${PROJECT_NAME}_LIBRARY_DIRS "${CMAKE_INSTALL_PREFIX}/lib ${ChimeraTK-DeviceAccess_LIBRARY_DIRS}")
set(${PROJECT_NAME}_CPPFLAGS "${ChimeraTK-DeviceAccess_CXX_FLAGS}")
set(${PROJECT_NAME}_LINKER_FLAGS "-Wl,-rpath=${CMAKE_INSTALL_PREFIX}/lib,--enable-new-dtags")
include(${CMAKE_SOURCE_DIR}/cmake/create_cmake_config_files.cmake)  

