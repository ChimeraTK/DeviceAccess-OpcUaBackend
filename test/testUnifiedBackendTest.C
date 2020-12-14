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

//#define BOOST_TEST_MODULE testUnifiedBackendTest
//#include <boost/test/included/unit_test.hpp>

#include <ChimeraTK/UnifiedBackendTest.h>

using namespace boost::unit_test_framework;
using namespace ChimeraTK;


//
//class OPCUALauncher : public ThreadedOPCUAServer{
//public:
//  OPCUALauncher(){
//    port = _server.drawPort();
//    path= "opcua://localhost:" + std::to_string(port);
//  }
//
//  static uint port;
//  static std::string path;
//};
//
//uint OPCUALauncher::port;
//std::string OPCUALauncher::path;

//static UA_Server *_server{nullptr};
//
//BOOST_GLOBAL_FIXTURE(OPCUALauncher)
//
//struct AllRegisterDefaults{
//  bool isWriteable() { return true; }
//  bool isReadable() { return true; }
//  ChimeraTK::AccessModeFlags supportedFlags() { return {}; }
//  size_t nChannels() { return 1; }
//  size_t writeQueueLength() { return std::numeric_limits<size_t>::max(); }
//  size_t nRuntimeErrorCases() { return 1; }
//  typedef std::nullptr_t rawUserType;
//
//  static constexpr auto capabilities = TestCapabilities<>()
//                                           .disableForceDataLossWrite()
//                                           .disableAsyncReadInconsistency()
//                                           .disableSwitchReadOnly()
//                                           .disableSwitchWriteOnly();
//};
//
//struct ScalarDefaults : AllRegisterDefaults{
//  using AllRegisterDefaults::AllRegisterDefaults;
//  size_t nElementsPerChannel() { return 1; }
//};
//
//struct ArrayDefaults : AllRegisterDefaults{
//  using AllRegisterDefaults::AllRegisterDefaults;
//  size_t nElementsPerChannel() { return 5; }
//};
//
//struct RegSomeInt : ScalarDefaults {
//  std::string path() { return "Dummy/scalar/int32"; }
//  typedef int32_t minimumUserType;
//  int32_t increment{3};
//
//  static constexpr auto capabilities = ScalarDefaults::capabilities.enableSwitchReadOnly();
//};


//BOOST_AUTO_TEST_CASE(unifiedBackendTest) {
//  auto ubt = ChimeraTK::UnifiedBackendTest<>()
//                 .addRegister<RegSomeInt>();
//
//  ubt.runTests("(" + OPCUALauncher::path + ")", "(" + OPCUALauncher::path + "?unused=parameter)");
//}
int main(){
  ThreadedOPCUAServer srv;
  std::cout << "Server was started..." << std::endl;
  for(size_t i = 0; i < 10; i++){
    std::this_thread::sleep_for(std::chrono::seconds(5));
    UA_Int32 data{i};
    std::cout << "Setting new value is " << data << std::endl;
    srv._server.setValue("Dummy/scalar/int32", data);
    UA_Variant* vData = srv._server.getValue(std::string("Dummy/scalar/int32"));
    UA_Int32* result = (UA_Int32*)vData->data;
    std::cout << "New value is " << *result << std::endl;
    UA_Variant_delete(vData);
  }
  std::cout << "Stopping the server." << std::endl;
  return 0;
}


