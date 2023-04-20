project(ChimeraTK-DeviceAccess-OPC-UA-Backend)
cmake_minimum_required(VERSION 2.8.13)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

set(${PROJECT_NAME}_MAJOR_VERSION 01)
set(${PROJECT_NAME}_MINOR_VERSION 03)
set(${PROJECT_NAME}_PATCH_VERSION 00)
include(cmake/set_version_numbers.cmake)

find_package( Threads )
find_package(Boost COMPONENTS unit_test_framework system)

find_package(ChimeraTK-DeviceAccess 03.09 REQUIRED)
find_package(open62541 01.03 REQUIRED)

# add support for latest C++ standard to compiler flags
include(cmake/enable_latest_cxx_support.cmake)
include_directories( include )

aux_source_directory(${CMAKE_SOURCE_DIR}/src library_sources)

add_library(${PROJECT_NAME} SHARED ${library_sources} )
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION} SOVERSION ${${PROJECT_NAME}_SOVERSION})
target_link_libraries(${PROJECT_NAME} 
    PUBLIC ChimeraTK::ChimeraTK-DeviceAccess
    PUBLIC open62541::open62541) 
# --no-as-needed: force linking against this library. This is required for proper registering.
target_link_options(${PROJECT_NAME} PUBLIC "-Wl,--no-as-needed")
# do not remove runtime paths of the library when installing (helps for unsually located implicit dependencies)
set_property(TARGET ${PROJECT_NAME} PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)

if(Boost_UNIT_TEST_FRAMEWORK_FOUND)
  ENABLE_TESTING()
 
  add_executable(testSimpleServer ${CMAKE_SOURCE_DIR}/test/DummyServer/DummyServer.cc ${CMAKE_SOURCE_DIR}/test/testSimpleServer.C )
  target_link_libraries( testSimpleServer 
        PRIVATE open62541::open62541 ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})
  target_include_directories(testSimpleServer 
        PRIVATE ${CMAKE_SOURCE_DIR}/test/DummyServer)
  add_test(testClient testSimpleServer)
  
  add_executable(testBrowse ${CMAKE_SOURCE_DIR}/test/testBrowse.C )
  target_link_libraries( testBrowse
        PRIVATE open62541::open62541 ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})
  add_test(testBrowse testBrowse)
  
  add_executable(testSubscription ${CMAKE_SOURCE_DIR}/test/testSubscription.C )
  target_link_libraries( testSubscription 
        PRIVATE open62541::open62541 ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})
  
  add_executable(testUnifiedBackendTest ${CMAKE_SOURCE_DIR}/test/DummyServer/DummyServer.cc ${CMAKE_SOURCE_DIR}/test/testUnifiedBackendTest.C )
  target_link_libraries( testUnifiedBackendTest 
        PRIVATE ${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})
  target_include_directories(testUnifiedBackendTest PRIVATE ${CMAKE_SOURCE_DIR}/test/DummyServer)
  add_test(testUnifiedBackendTest testUnifiedBackendTest)

  add_executable(memoryTest ${CMAKE_SOURCE_DIR}/test/memorytest.C )
  target_link_libraries( memoryTest open62541::open62541)

else()
  message(STATUS "Boost unit test framework not found. No tests will be build.")
endif()

#Install the library and the executables
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
            EXPORT ${PROJECT_NAME}Targets
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
  
#all include files go into include/PROJECT_NAME
#The exclusion of ${PROJECT_NAME} prevents the recursive installation of the files just being installed.
#The original headers are in include/*.h, the installed ones in include/PROJECT_NAME/*.h.
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include/${PROJECT_NAME}
          FILES_MATCHING PATTERN "*.h"
          PATTERN ".svn" EXCLUDE
          PATTERN "${PROJECT_NAME}" EXCLUDE
          )

# we support our cmake EXPORTS as imported targets
set(PROVIDES_EXPORTED_TARGETS 1)
# we need the public dependencies so create_cmake_config_files can find them as implicit dependencies
list(APPEND ${PROJECT_NAME}_PUBLIC_DEPENDENCIES "open62541;ChimeraTK-DeviceAccess")
include(${CMAKE_SOURCE_DIR}/cmake/create_cmake_config_files.cmake)  
