/*
 * testUnifiedBackendTest.C
 *
 *  Created on: Dec 9, 2020
 *      Author: Klaus Zenker (HZDR)
 */

#include <iostream>
#include <chrono>
#include <cstddef>

#include "DummyServer.h"
#include "open62541.h"

#define BOOST_TEST_MODULE testUnifiedBackendTest
#include <boost/test/included/unit_test.hpp>

#include <ChimeraTK/UnifiedBackendTest.h>

using namespace boost::unit_test_framework;
using namespace ChimeraTK;



class OPCUALauncher {
public:
  OPCUALauncher(){
    port = server._server.getPort();
    path= "opcua:localhost";
    threadedServer = &server;
    server.start();
  }

  uint port;
  std::string path;
  ThreadedOPCUAServer server;
  static ThreadedOPCUAServer* threadedServer;
};

ThreadedOPCUAServer* OPCUALauncher::threadedServer;

struct AllRegisterDefaults{
  bool isWriteable() { return true; }
  bool isReadable() { return true; }
  ChimeraTK::AccessModeFlags supportedFlags() { return {}; }
  size_t nChannels() { return 1; }
  size_t writeQueueLength() { return std::numeric_limits<size_t>::max(); }
  size_t nRuntimeErrorCases() { return 1; }
  typedef std::nullptr_t rawUserType;
  typedef int32_t minimumUserType;

  static constexpr auto capabilities = TestCapabilities<>()
                                           .disableForceDataLossWrite()
                                           .disableAsyncReadInconsistency()
                                           .disableSwitchReadOnly()
                                           .disableSwitchWriteOnly();
  void setForceRuntimeError(bool enable, size_t){
    if(enable){
      OPCUALauncher::threadedServer->_server.stop();
      // check if the server is really off
      if(!OPCUALauncher::threadedServer->checkConnection(ServerState::Off)){
        throw std::runtime_error("Failed to force runtime error.");
      }
      std::cout << "Server is stopped." << std::endl;
    }
    else {
      // check if server is running is done by the method itself.
      OPCUALauncher::threadedServer->start();
    }
    // sleep for twice the publishing interval
    std::this_thread::sleep_for(std::chrono::milliseconds(2*100));

  }



};

struct ScalarDefaults : AllRegisterDefaults{
  using AllRegisterDefaults::AllRegisterDefaults;
  size_t nElementsPerChannel() { return 1; }
};

struct ArrayDefaults : AllRegisterDefaults{
  using AllRegisterDefaults::AllRegisterDefaults;
  size_t nElementsPerChannel() { return 5; }
};

struct RegSomeInt : ScalarDefaults {
  AccessModeFlags supportedFlags() { return {AccessMode::wait_for_new_data}; }
  std::string path() { return "Dummy/scalar/int32"; }
  typedef int32_t minimumUserType;
  int32_t increment{3};
  UA_Int32* data;

  // \ToDo: Is that needed for OPC UA?
//  static constexpr auto capabilities = ScalarDefaults::capabilities.enableAsyncReadInconsistency();

  template<typename UserType>
  std::vector<std::vector<UserType> > generateValue(){
    auto currentData = getRemoteValue<UserType>();
    UserType data = currentData.at(0).at(0) + increment;
    return {{data}};
  }

  template<typename UserType>
  std::vector<std::vector<UserType> > getRemoteValue(){
    auto variant = OPCUALauncher::threadedServer->_server.getValue(path());
    data = (UA_Int32*)variant->data;
    auto d = ChimeraTK::numericToUserType<UserType>(*data);
    UA_Variant_delete(variant);
    return {{d}};
  }

  void setRemoteValue(){
    UA_Int32 value = generateValue<int32_t>().at(0).at(0);
    std::cout << "Setting value: " << value << std::endl;
    OPCUALauncher::threadedServer->_server.setValue(path(),value);
  }

};

// use test fixture suite to have access to the fixture class members
BOOST_FIXTURE_TEST_SUITE(s, OPCUALauncher)
BOOST_AUTO_TEST_CASE(unifiedBackendTest) {
  auto ubt = ChimeraTK::UnifiedBackendTest<>()
                 .addRegister<RegSomeInt>();
  // wait for the server to come up
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::stringstream ss;
  // minimum publishing interval on the server is 100ms
  ss << "(" << path << "?port=" << port << "&publishingInterval=100)";

  ubt.runTests(ss.str());
}

BOOST_AUTO_TEST_SUITE_END()
//int main(){
//  ThreadedOPCUAServer srv;
//  std::cout << "Server was started..." << std::endl;
//  for(size_t i = 0; i < 10; i++){
//    std::this_thread::sleep_for(std::chrono::seconds(5));
//    UA_Int32 data{i};
//    std::cout << "Setting new value is " << data << std::endl;
//    srv._server.setValue("Dummy/scalar/int32", data);
//    UA_Variant* vData = srv._server.getValue(std::string("Dummy/scalar/int32"));
//    UA_Int32* result = (UA_Int32*)vData->data;
//    std::cout << "New value is " << *result << std::endl;
//    UA_Variant_delete(vData);
//  }
//  std::cout << "Stopping the server." << std::endl;
//  return 0;
//}


