add_executable(testSimpleServer ${CMAKE_SOURCE_DIR}/test/DummyServer/DummyServer.cc ${CMAKE_SOURCE_DIR}/test/testSimpleServer.C)
target_link_libraries(testSimpleServer
      PRIVATE open62541::open62541 ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})
target_include_directories(testSimpleServer
      PRIVATE ${CMAKE_SOURCE_DIR}/test/DummyServer)
add_test(testClient testSimpleServer)

add_executable(testBrowse ${CMAKE_SOURCE_DIR}/test/testBrowse.C)
target_link_libraries(testBrowse
      PRIVATE open62541::open62541 ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})
add_test(testBrowse testBrowse)

FILE(COPY ${CMAKE_SOURCE_DIR}/test/opcua_map.map 
     ${CMAKE_SOURCE_DIR}/test/opcua_map_rootNode.map
     ${CMAKE_SOURCE_DIR}/test/opcua_map_xml.map
     ${CMAKE_SOURCE_DIR}/test/opcua_map_xml_rootNode.map
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_executable(testMapFiles ${CMAKE_SOURCE_DIR}/test/DummyServer/DummyServer.cc ${CMAKE_SOURCE_DIR}/test/testMapFiles.C)
target_link_libraries(testMapFiles
      PRIVATE open62541::open62541 ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES} ChimeraTK::ChimeraTK-DeviceAccess ${PROJECT_NAME})
target_include_directories(testMapFiles
      PRIVATE ${CMAKE_SOURCE_DIR}/test/DummyServer)

add_test(testMapFiles testMapFiles)


add_executable(testSubscription ${CMAKE_SOURCE_DIR}/test/testSubscription.C)
target_link_libraries(testSubscription
      PRIVATE open62541::open62541 ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})

add_executable(testUnifiedBackendTest ${CMAKE_SOURCE_DIR}/test/DummyServer/DummyServer.cc ${CMAKE_SOURCE_DIR}/test/testUnifiedBackendTest.C)
target_link_libraries(testUnifiedBackendTest
      PRIVATE ${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES} open62541::open62541)
target_include_directories(testUnifiedBackendTest PRIVATE ${CMAKE_SOURCE_DIR}/test/DummyServer)
add_test(testUnifiedBackendTest testUnifiedBackendTest)

add_executable(memoryTest ${CMAKE_SOURCE_DIR}/test/memorytest.C)
target_link_libraries(memoryTest open62541::open62541)