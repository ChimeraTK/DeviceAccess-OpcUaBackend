/*
 * testUnifiedBackendTest.C
 *
 *  Created on: Dec 9, 2020
 *      Author: Klaus Zenker (HZDR)
 */

#include <chrono>
#include <cstddef>

#include "DummyServer.h"
#include <open62541/plugin/log_stdout.h>

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
  virtual bool isWriteable() { return true; }
  bool isReadable() { return true; }
  AccessModeFlags supportedFlags() { return {AccessMode::wait_for_new_data}; }
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
      UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                        "Server is stopped.");
    }
    else {
      // check if server is running is done by the method itself.
      OPCUALauncher::threadedServer->start();
    }
    // sleep for twice the publishing interval
    std::this_thread::sleep_for(std::chrono::milliseconds(2*100));
  }
};

template <typename UAType>
struct ScalarDefaults : AllRegisterDefaults{
  using AllRegisterDefaults::AllRegisterDefaults;
  size_t nElementsPerChannel() { return 1; }
  virtual std::string path() = 0;

  // \ToDo: Is that needed for OPC UA?
//  static constexpr auto capabilities = ScalarDefaults::capabilities.enableAsyncReadInconsistency();

  UAType* data;
  
  template<typename UserType>
  std::vector<std::vector<UserType> > generateValue(){
    UserType increment(3);
    auto currentData = getRemoteValue<UserType>();
    UserType data = currentData.at(0).at(0) + increment;
    return {{data}};
  }
  
  template<typename UserType>
  std::vector<std::vector<UserType> > getRemoteValue(){
    auto variant = OPCUALauncher::threadedServer->_server.getValue(path());
    data = (UAType*)variant->data;
    auto d = ChimeraTK::numericToUserType<UserType>(*data);
    UA_Variant_delete(variant);
    return {{d}};
  }

  void setRemoteValue(){
    std::vector<UAType> value;
    //\ToDo: Should this be UserType instead of UAType?
    value.push_back(generateValue<UAType>().at(0).at(0));
    std::stringstream ss;
    ss << value.at(0);
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                      "Setting value:   %s", ss.str().c_str());
    OPCUALauncher::threadedServer->_server.setValue(path(),value);
  }
};

template <typename UAType>
struct ScalarDefaultsRO : ScalarDefaults<UAType>{
  bool isWriteable() override {return false;};
};


template<typename T>
struct identity { typedef T type; };

template <>
struct ScalarDefaults<UA_String> : AllRegisterDefaults{
  using AllRegisterDefaults::AllRegisterDefaults;
  size_t nElementsPerChannel() { return 1; }
  virtual std::string path() = 0;

  // \ToDo: Is that needed for OPC UA?
//  static constexpr auto capabilities = ScalarDefaults::capabilities.enableAsyncReadInconsistency();

  UA_String* data;

  template<typename UserType>
  std::vector<std::vector<UserType> > generateValue(){
    return generateValue(identity<UserType>());
  }

  template<typename UserType>
  std::vector<std::vector<UserType> > getRemoteValue(){
    auto variant = OPCUALauncher::threadedServer->_server.getValue(path());
    data = (UA_String*)variant->data;
    std::stringstream ss(std::string((char*)data->data, data->length));
    int tmpInt;
    ss >> tmpInt;
    auto d = ChimeraTK::numericToUserType<UserType>(tmpInt);
    UA_Variant_delete(variant);
    return {{d}};
  }

  void setRemoteValue(){
    std::vector<UA_String> value;
    std::string tmp  = generateValue<std::string>().at(0).at(0);
    value.push_back(UA_STRING((char*)tmp.c_str()));
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                      "Setting value:   %s", tmp.c_str());
    OPCUALauncher::threadedServer->_server.setValue(path(),value);
  }

  private:
  template<typename UserType>
  std::vector<std::vector<UserType> > generateValue(identity<UserType>){
    UserType increment(3);
    auto currentData = getRemoteValue<UserType>();
    UserType data = currentData.at(0).at(0) + increment;
    return {{data}};
  }
  std::vector<std::vector<std::string> > generateValue(identity<std::string>){
    int increment(3);
    auto currentData = getRemoteValue<int>();
    int data = currentData.at(0).at(0) + increment;
    return {{std::to_string(data)}};
  }

};

template <>
struct ScalarDefaultsRO<UA_String> : ScalarDefaults<UA_String>{
  bool isWriteable() override {return false;};
};

template <>
struct ScalarDefaults<UA_Boolean> : AllRegisterDefaults{
  using AllRegisterDefaults::AllRegisterDefaults;
  size_t nElementsPerChannel() { return 1; }
  virtual std::string path() = 0;

  // \ToDo: Is that needed for OPC UA?
//  static constexpr auto capabilities = ScalarDefaults::capabilities.enableAsyncReadInconsistency();

  UA_Boolean* data;

  template<typename UserType>
  std::vector<std::vector<UserType> > getRemoteValue(){
    auto variant = OPCUALauncher::threadedServer->_server.getValue(path());
    data = (UA_Boolean*)variant->data;
    // convert to int
    int idata = *data;
    auto d = ChimeraTK::numericToUserType<UserType>(idata);
    UA_Variant_delete(variant);
    return {{d}};
  }

  void setRemoteValue(){
    std::vector<UA_Boolean> value;
    value.push_back(generateValue<int>().at(0).at(0));
    std::stringstream ss;
    ss << value.at(0);
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                      "Setting value:   %s", ss.str().c_str());
    OPCUALauncher::threadedServer->_server.setValue(path(),value);
  }

  template<typename UserType>
  std::vector<std::vector<UserType> > generateValue(){
    UserType increment(1);
    auto currentData = getRemoteValue<UserType>();
    if(currentData.at(0).at(0) > 0 ){
      UserType data = currentData.at(0).at(0) - increment;
      return {{data}};
    } else {
      UserType data = currentData.at(0).at(0) + increment;
      return {{data}};
    }
  }

};

template <>
struct ScalarDefaultsRO<UA_Boolean> : ScalarDefaults<UA_Boolean>{
  bool isWriteable() override {return false;};
};


template <typename UAType>
struct ArrayDefaults : AllRegisterDefaults{
  using AllRegisterDefaults::AllRegisterDefaults;
  size_t nElementsPerChannel() { return 5; }
  virtual std::string path() = 0;
  UAType* data;

  // \ToDo: Is that needed for OPC UA?
//  static constexpr auto capabilities = ScalarDefaults::capabilities.enableAsyncReadInconsistency();

  template<typename UserType>
  std::vector<std::vector<UserType> > generateValue(){
    auto currentData = getRemoteValue<UserType>();
    UserType increment(3);
    std::vector<UserType> val;
    for(size_t i =0; i < nElementsPerChannel(); ++i){
      val.push_back(currentData.at(0).at(i)+(i+1)*increment);
    }
    return {val};
  }

  template<typename UserType>
  std::vector<std::vector<UserType> > getRemoteValue(){
    auto variant = OPCUALauncher::threadedServer->_server.getValue(path());
    data = (UAType*)variant->data;
    std::vector<UserType> values;
    for(size_t i =0; i < nElementsPerChannel(); ++i){
      auto value = ChimeraTK::numericToUserType<UserType>(data[i]);
      values.push_back(value);
    }
    UA_Variant_delete(variant);
    return {values};
  }

  void setRemoteValue(){
    std::vector<UAType> values;
    //\ToDo: Should this be UserType instead of UAType?
    auto v = generateValue<UAType>().at(0);
    std::stringstream ss;
    for(auto t : v){
      values.push_back(t);
      ss << " " << t;
    }
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                      "Setting array:   %s", ss.str().c_str());
    OPCUALauncher::threadedServer->_server.setValue(path(),values, nElementsPerChannel());
  }
};

template <typename UAType>
struct ArrayDefaultsRO : ArrayDefaults<UAType>{
 bool isWriteable() override {return false;}
};

template <>
struct ArrayDefaults<UA_String> : AllRegisterDefaults{
  using AllRegisterDefaults::AllRegisterDefaults;
  size_t nElementsPerChannel() { return 5; }
  virtual std::string path() = 0;
  UA_String* data;

  // \ToDo: Is that needed for OPC UA?
//  static constexpr auto capabilities = ScalarDefaults::capabilities.enableAsyncReadInconsistency();

  template<typename UserType>
  std::vector<std::vector<UserType> > generateValue(){
    return generateValue(identity<UserType>());
  }

  template<typename UserType>
  std::vector<std::vector<UserType> > getRemoteValue(){
    auto variant = OPCUALauncher::threadedServer->_server.getValue(path());
    data = (UA_String*)variant->data;
    std::vector<UserType> values;
    int tmpInt;
    for(size_t i =0; i < nElementsPerChannel(); ++i){
      std::string tmp = std::string((char*)data[i].data, data[i].length);
      std::stringstream ss(std::string((char*)data[i].data, data[i].length));
      ss >> tmpInt;
      auto value = ChimeraTK::numericToUserType<UserType>(tmpInt);
      values.push_back(value);
    }
    UA_Variant_delete(variant);
    return {values};
  }

  void setRemoteValue(){
    std::vector<UA_String> values;
    //\ToDo: Should this be UserType instead of UAType?
    auto v = generateValue<std::string>().at(0);
    std::stringstream ss;
    for(auto &t : v){
      values.push_back(UA_STRING((char*)t.c_str()));
      ss << " " << t;
    }
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                      "Setting array:   %s", ss.str().c_str());
    OPCUALauncher::threadedServer->_server.setValue(path(),values, nElementsPerChannel());
  }

private:
  template<typename UserType>
  std::vector<std::vector<UserType> > generateValue(identity<UserType>){
    auto currentData = getRemoteValue<UserType>();
    UserType increment(3);
    std::vector<UserType> val;
    for(size_t i =0; i < nElementsPerChannel(); ++i){
      val.push_back(currentData.at(0).at(i)+(i+1)*increment);
    }
    return {val};
  }

  std::vector<std::vector<std::string> > generateValue(identity<std::string>){
    auto currentData = getRemoteValue<int>();
    int increment(3);
    std::vector<std::string> val;
    for(size_t i =0; i < nElementsPerChannel(); ++i){
      val.push_back(std::to_string(currentData.at(0).at(i)+(i+1)*increment));
    }
    return {val};
  }
};

template <>
struct ArrayDefaultsRO<UA_String> : ArrayDefaults<UA_String>{
 bool isWriteable() override {return false;}
};

template <>
struct ArrayDefaults<UA_Boolean> : AllRegisterDefaults{
  using AllRegisterDefaults::AllRegisterDefaults;
  size_t nElementsPerChannel() { return 5; }
  virtual std::string path() = 0;
  UA_Boolean* data;

  // \ToDo: Is that needed for OPC UA?
//  static constexpr auto capabilities = ScalarDefaults::capabilities.enableAsyncReadInconsistency();

  template<typename UserType>
  std::vector<std::vector<UserType> > getRemoteValue(){
    auto variant = OPCUALauncher::threadedServer->_server.getValue(path());
    data = (UA_Boolean*)variant->data;
    // convert to int
    int idata;
    std::vector<UserType> d;
    for(size_t i = 0; i < nElementsPerChannel(); ++i){
      idata = data[i];
      d.push_back(ChimeraTK::numericToUserType<UserType>(idata));
    }
    return {d};
  }

  void setRemoteValue(){
    std::vector<UA_Boolean> values;
    //\ToDo: Should this be UserType instead of UAType?
    auto v = generateValue<int>().at(0);
    std::stringstream ss;
    for(auto &t : v){
      values.push_back(t);
      ss << " " << t;
    }
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                      "Setting array:   %s", ss.str().c_str());
    OPCUALauncher::threadedServer->_server.setValue(path(),values, nElementsPerChannel());
  }

  template<typename UserType>
  std::vector<std::vector<UserType> > generateValue(){
    auto currentData = getRemoteValue<UserType>();
    UserType increment(1);
    std::vector<UserType> val;
    for(size_t i =0; i < nElementsPerChannel(); ++i){
      if(currentData.at(0).at(i) > 0)
        val.push_back(currentData.at(0).at(i) - increment);
      else
        val.push_back(currentData.at(0).at(i) + increment);
    }
    return {val};
  }
};

template <>
struct ArrayDefaultsRO<UA_Boolean> : ArrayDefaults<UA_Boolean>{
 bool isWriteable() override {return false;}
};

struct RegSomeBool : ScalarDefaults<UA_Boolean> {
  std::string path() override { return "Dummy/scalar/bool"; }
  typedef uint16_t minimumUserType;
};

struct RegSomeInt64 : ScalarDefaults<UA_Int64> {
  std::string path() override { return "Dummy/scalar/int64"; }
  typedef int64_t minimumUserType;
};

struct RegSomeUInt64 : ScalarDefaults<UA_UInt64> {
  std::string path() override { return "Dummy/scalar/uint64"; }
  typedef uint64_t minimumUserType;
};

struct RegSomeInt32 : ScalarDefaults<UA_Int32> {
  std::string path() override { return "Dummy/scalar/int32"; }
  typedef int32_t minimumUserType;
};

struct RegSomeUInt32 : ScalarDefaults<UA_UInt32> {
  std::string path() override { return "Dummy/scalar/uint32"; }
  typedef uint32_t minimumUserType;
};

struct RegSomeInt16 : ScalarDefaults<UA_Int16> {
  std::string path() override { return "Dummy/scalar/int16"; }
  typedef int16_t minimumUserType;
};

struct RegSomeUInt16 : ScalarDefaults<UA_UInt16> {
  std::string path() override { return "Dummy/scalar/uint16"; }
  typedef uint16_t minimumUserType;
};

struct RegSomeFloat : ScalarDefaults<UA_Float> {
  std::string path() override { return "Dummy/scalar/float"; }
  typedef float minimumUserType;
};

struct RegSomeDouble : ScalarDefaults<UA_Double> {
  std::string path() override { return "Dummy/scalar/double"; }
  typedef double minimumUserType;
};

struct RegSomeString : ScalarDefaults<UA_String> {
  std::string path() override { return "Dummy/scalar/string"; }
  typedef std::string minimumUserType;
};

struct RegSomeBoolArray : ArrayDefaults<UA_Boolean> {
  std::string path() override { return "Dummy/array/bool"; }
  typedef uint16_t minimumUserType;
};


struct RegSomeInt64Array : ArrayDefaults<UA_Int64> {
  std::string path() override { return "Dummy/array/int64"; }
  typedef int64_t minimumUserType;
};

struct RegSomeUInt64Array : ArrayDefaults<UA_UInt64> {
  std::string path() override { return "Dummy/array/uint64"; }
  typedef uint64_t minimumUserType;
};

struct RegSomeInt32Array : ArrayDefaults<UA_Int32> {
  std::string path() override { return "Dummy/array/int32"; }
  typedef int32_t minimumUserType;
};

struct RegSomeUInt32Array : ArrayDefaults<UA_UInt32> {
  std::string path() override { return "Dummy/array/uint32"; }
  typedef uint32_t minimumUserType;
};

struct RegSomeInt16Array : ArrayDefaults<UA_Int16> {
  std::string path() override { return "Dummy/array/int16"; }
  typedef int16_t minimumUserType;
};

struct RegSomeUInt16Array : ArrayDefaults<UA_UInt16> {
  std::string path() override { return "Dummy/array/uint16"; }
  typedef uint16_t minimumUserType;
};

struct RegSomeFloatArray : ArrayDefaults<UA_Float> {
  std::string path() override { return "Dummy/array/float"; }
  typedef float minimumUserType;
};

struct RegSomeDoubleArray : ArrayDefaults<UA_Double> {
  std::string path() override { return "Dummy/array/double"; }
  typedef double minimumUserType;
};

struct RegSomeStringArray : ArrayDefaults<UA_String> {
  std::string path() override { return "Dummy/array/string"; }
  typedef std::string minimumUserType;
};

// read only part
struct RegSomeBoolRO : ScalarDefaultsRO<UA_Boolean> {
  std::string path() override { return "Dummy/scalar_ro/bool"; }
  typedef uint16_t minimumUserType;
};

struct RegSomeInt64RO : ScalarDefaultsRO<UA_Int64> {
  std::string path() override { return "Dummy/scalar_ro/int64"; }
  typedef int64_t minimumUserType;
};

struct RegSomeUInt64RO : ScalarDefaultsRO<UA_UInt64> {
  std::string path() override { return "Dummy/scalar_ro/uint64"; }
  typedef uint64_t minimumUserType;
};

struct RegSomeInt32RO : ScalarDefaultsRO<UA_Int32> {
  std::string path() override { return "Dummy/scalar_ro/int32"; }
  typedef int32_t minimumUserType;
};

struct RegSomeUInt32RO : ScalarDefaultsRO<UA_UInt32> {
  std::string path() override { return "Dummy/scalar_ro/uint32"; }
  typedef uint32_t minimumUserType;
};

struct RegSomeInt16RO : ScalarDefaultsRO<UA_Int16> {
  std::string path() override { return "Dummy/scalar_ro/int16"; }
  typedef int16_t minimumUserType;
};

struct RegSomeUInt16RO : ScalarDefaultsRO<UA_UInt16> {
  std::string path() override { return "Dummy/scalar_ro/uint16"; }
  typedef uint16_t minimumUserType;
};

struct RegSomeFloatRO : ScalarDefaultsRO<UA_Float> {
  std::string path() override { return "Dummy/scalar_ro/float"; }
  typedef float minimumUserType;
};

struct RegSomeDoubleRO : ScalarDefaultsRO<UA_Double> {
  std::string path() override { return "Dummy/scalar_ro/double"; }
  typedef double minimumUserType;
};

struct RegSomeStringRO : ScalarDefaultsRO<UA_String> {
  std::string path() override { return "Dummy/scalar_ro/string"; }
  typedef std::string minimumUserType;
};

struct RegSomeBoolArrayRO : ArrayDefaultsRO<UA_Boolean> {
  std::string path() override { return "Dummy/array_ro/bool"; }
  typedef uint16_t minimumUserType;
};

struct RegSomeInt64ArrayRO : ArrayDefaultsRO<UA_Int64> {
  std::string path() override { return "Dummy/array_ro/int64"; }
  typedef int64_t minimumUserType;
};

struct RegSomeUInt64ArrayRO : ArrayDefaultsRO<UA_UInt64> {
  std::string path() override { return "Dummy/array_ro/uint64"; }
  typedef uint64_t minimumUserType;
};

struct RegSomeInt32ArrayRO : ArrayDefaultsRO<UA_Int32> {
  std::string path() override { return "Dummy/array_ro/int32"; }
  typedef int32_t minimumUserType;
};

struct RegSomeUInt32ArrayRO : ArrayDefaultsRO<UA_UInt32> {
  std::string path() override { return "Dummy/array_ro/uint32"; }
  typedef uint32_t minimumUserType;
};

struct RegSomeInt16ArrayRO : ArrayDefaultsRO<UA_Int16> {
  std::string path() override { return "Dummy/array_ro/int16"; }
  typedef int16_t minimumUserType;
};

struct RegSomeUInt16ArrayRO : ArrayDefaultsRO<UA_UInt16> {
  std::string path() override { return "Dummy/array_ro/uint16"; }
  typedef uint16_t minimumUserType;
};

struct RegSomeFloatArrayRO : ArrayDefaultsRO<UA_Float> {
  std::string path() override { return "Dummy/array_ro/float"; }
  typedef float minimumUserType;
};

struct RegSomeDoubleArrayRO : ArrayDefaultsRO<UA_Double> {
  std::string path() override { return "Dummy/array_ro/double"; }
  typedef double minimumUserType;
};

struct RegSomeStringArrayRO : ArrayDefaultsRO<UA_String> {
  std::string path() override { return "Dummy/array_ro/string"; }
  typedef std::string minimumUserType;
};

// use test fixture suite to have access to the fixture class members
BOOST_FIXTURE_TEST_SUITE(s, OPCUALauncher)
BOOST_AUTO_TEST_CASE(unifiedBackendTest) {
//  auto ubt = ChimeraTK::UnifiedBackendTest<>()
//                 .addRegister<RegSomeInt16>()
//                 .addRegister<RegSomeBool>()
//                 .addRegister<RegSomeUInt16>()
//                 .addRegister<RegSomeInt32>()
//                 .addRegister<RegSomeUInt32>()
//                 .addRegister<RegSomeInt64>()
//                 .addRegister<RegSomeUInt64>()
//                 .addRegister<RegSomeFloat>()
//                 .addRegister<RegSomeDouble>()
//                 .addRegister<RegSomeString>()
//                 .addRegister<RegSomeBoolArray>()
//                 .addRegister<RegSomeInt16Array>()
//                 .addRegister<RegSomeUInt16Array>()
//                 .addRegister<RegSomeInt32Array>()
//                 .addRegister<RegSomeUInt32Array>()
//                 .addRegister<RegSomeInt64Array>()
//                 .addRegister<RegSomeUInt64Array>()
//                 .addRegister<RegSomeFloatArray>()
//                 .addRegister<RegSomeDoubleArray>()
//                 .addRegister<RegSomeStringArray>()
//                 .addRegister<RegSomeBoolRO>()
//                 .addRegister<RegSomeInt16RO>()
//                 .addRegister<RegSomeUInt16RO>()
//                 .addRegister<RegSomeInt32RO>()
//                 .addRegister<RegSomeUInt32RO>()
//                 .addRegister<RegSomeInt64RO>()
//                 .addRegister<RegSomeUInt64RO>()
//                 .addRegister<RegSomeFloatRO>()
//                 .addRegister<RegSomeDoubleRO>()
//                 .addRegister<RegSomeStringRO>()
//                 .addRegister<RegSomeBoolArrayRO>()
//                 .addRegister<RegSomeInt16ArrayRO>()
//                 .addRegister<RegSomeUInt16ArrayRO>()
//                 .addRegister<RegSomeInt32ArrayRO>()
//                 .addRegister<RegSomeUInt32ArrayRO>()
//                 .addRegister<RegSomeInt64ArrayRO>()
//                 .addRegister<RegSomeUInt64ArrayRO>()
//                 .addRegister<RegSomeFloatArrayRO>()
//                 .addRegister<RegSomeDoubleArrayRO>()
//                 .addRegister<RegSomeStringArrayRO>();
//  auto ubt = ChimeraTK::UnifiedBackendTest<>().addRegister<RegSomeBoolArray>().addRegister<RegSomeBool>().addRegister<RegSomeBoolArrayRO>().addRegister<RegSomeBoolRO>();
  auto ubt = ChimeraTK::UnifiedBackendTest<>().addRegister<RegSomeUInt32>();
  // wait for the server to come up
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::stringstream ss;
  // minimum publishing interval on the server is 100ms
  ss << "(" << path << "?port=" << port << "&publishingInterval=100)";

  ubt.runTests(ss.str(),"");
}

BOOST_AUTO_TEST_SUITE_END()
