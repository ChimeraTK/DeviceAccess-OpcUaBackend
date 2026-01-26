// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * testUnifiedBackendTest.C
 *
 *  Created on: Dec 9, 2020
 *      Author: Klaus Zenker (HZDR)
 */

#include "DummyServer.h"

#include <open62541/plugin/log_stdout.h>

#include <chrono>
#include <cstddef>

#define BOOST_TEST_MODULE testUnifiedBackendTest
#include <ChimeraTK/UnifiedBackendTest.h>

#include <boost/test/unit_test.hpp>

using namespace boost::unit_test_framework;
using namespace ChimeraTK;

class OPCUALauncher {
 public:
  OPCUALauncher() {
    port = server.server.getPort();
    path = "opcua:localhost";
    threadedServer = &server;
    server.start();
  }

  uint port;
  std::string path;
  ThreadedOPCUAServer server;
  static ThreadedOPCUAServer* threadedServer;
};

ThreadedOPCUAServer* OPCUALauncher::threadedServer;

struct AllRegisterDefaults {
  virtual ~AllRegisterDefaults() {}
  virtual bool isWriteable() { return true; }
  static bool isReadable() { return true; }
  static AccessModeFlags supportedFlags() { return {AccessMode::wait_for_new_data}; }
  static size_t nChannels() { return 1; }
  static size_t writeQueueLength() { return std::numeric_limits<size_t>::max(); }
  static size_t nRuntimeErrorCases() { return 2; }
  using rawType = std::nullptr_t;
  using maximumUserType = int32_t;

  static constexpr auto capabilities = TestCapabilities<>()
                                           .disableForceDataLossWrite()
                                           .disableAsyncReadInconsistency()
                                           .disableSwitchReadOnly()
                                           .disableSwitchWriteOnly()
                                           .disableTestRawTransfer();

  static void setForceRuntimeError(bool enable, size_t test) {
    switch(test) {
      case 1:
        if(enable) {
          OPCUALauncher::threadedServer->server.stop();
          // check if the server is really off
          if(!OPCUALauncher::threadedServer->checkConnection(ServerState::Off)) {
            throw std::runtime_error("Failed to force runtime error.");
          }
          UA_LOG_DEBUG(&OPCUAServer::logger, UA_LOGCATEGORY_USERLAND, "Server is stopped.");
        }
        else {
          // check if server is running is done by the method itself.
          OPCUALauncher::threadedServer->start();
        }
        // sleep for twice the publishing interval
        std::this_thread::sleep_for(std::chrono::milliseconds(2 * publishingInterval));
        break;
      case 0:
        if(enable) {
          UA_LOG_INFO(&OPCUAServer::logger, UA_LOGCATEGORY_USERLAND, "Locking server.");
          OPCUALauncher::threadedServer->server.mux.lock();
        }
        else {
          UA_LOG_INFO(&OPCUAServer::logger, UA_LOGCATEGORY_USERLAND, "Unlocking server.");
          OPCUALauncher::threadedServer->server.mux.unlock();
          std::this_thread::sleep_for(std::chrono::milliseconds(2 * publishingInterval));
        }
        break;
      default:
        throw std::runtime_error("Unknown error case.");
    }
  }
};

template<typename UAType>
struct ScalarDefaults : AllRegisterDefaults {
  using AllRegisterDefaults::AllRegisterDefaults;
  size_t nElementsPerChannel() { return 1; }
  virtual std::string path() = 0;

  // \ToDo: Is that needed for OPC UA?
  //  static constexpr auto capabilities = ScalarDefaults::capabilities.enableAsyncReadInconsistency();

  UAType* data;

  template<typename UserType>
  std::vector<std::vector<UserType>> generateValue() {
    UserType increment(3);
    auto currentData = getRemoteValue<UserType>();
    UserType data = currentData.at(0).at(0) + increment;
    return {{data}};
  }

  template<typename UserType>
  std::vector<std::vector<UserType>> getRemoteValue() {
    auto variant = OPCUALauncher::threadedServer->server.getValue(path());
    data = (UAType*)variant->data;
    auto d = ChimeraTK::numericToUserType<UserType>(*data);
    UA_Variant_delete(variant);
    return {{d}};
  }

  void setRemoteValue() {
    std::vector<UAType> value;
    //\ToDo: Should this be UserType instead of UAType?
    value.push_back(generateValue<UAType>().at(0).at(0));
    std::stringstream ss;
    ss << value.at(0);
    UA_LOG_DEBUG(&OPCUAServer::logger, UA_LOGCATEGORY_USERLAND, "Setting value:   %s", ss.str().c_str());
    OPCUALauncher::threadedServer->server.setValue(path(), value);
    std::this_thread::sleep_for(std::chrono::milliseconds(2 * publishingInterval));
  }
};

template<typename UAType>
struct ScalarDefaultsRO : ScalarDefaults<UAType> {
  bool isWriteable() override { return false; };
};

template<typename T>
struct Identity {
  using type = T;
};

template<>
struct ScalarDefaults<UA_String> : AllRegisterDefaults {
  using AllRegisterDefaults::AllRegisterDefaults;
  static size_t nElementsPerChannel() { return 1; }
  virtual std::string path() = 0;

  // \ToDo: Is that needed for OPC UA?
  //  static constexpr auto capabilities = ScalarDefaults::capabilities.enableAsyncReadInconsistency();

  UA_String* data;

  template<typename UserType>
  std::vector<std::vector<UserType>> generateValue() {
    return generateValue(Identity<UserType>());
  }

  template<typename UserType>
  std::vector<std::vector<UserType>> getRemoteValue() {
    auto* variant = OPCUALauncher::threadedServer->server.getValue(path());
    data = (UA_String*)variant->data;
    std::stringstream ss(std::string((char*)data->data, data->length));
    int tmpInt;
    ss >> tmpInt;
    auto d = ChimeraTK::numericToUserType<UserType>(tmpInt);
    UA_Variant_delete(variant);
    return {{d}};
  }

  void setRemoteValue() {
    std::vector<UA_String> value;
    std::string tmp = generateValue<std::string>().at(0).at(0);
    value.push_back(UA_STRING((char*)tmp.c_str()));
    UA_LOG_DEBUG(&OPCUAServer::logger, UA_LOGCATEGORY_USERLAND, "Setting value:   %s", tmp.c_str());
    OPCUALauncher::threadedServer->server.setValue(path(), value);
    std::this_thread::sleep_for(std::chrono::milliseconds(2 * publishingInterval));
  }

 private:
  template<typename UserType>
  std::vector<std::vector<UserType>> generateValue(Identity<UserType>) {
    UserType increment(3);
    auto currentData = getRemoteValue<UserType>();
    UserType data = currentData.at(0).at(0) + increment;
    return {{data}};
  }
  std::vector<std::vector<std::string>> generateValue(Identity<std::string>) {
    int increment(3);
    auto currentData = getRemoteValue<int>();
    int data = currentData.at(0).at(0) + increment;
    return {{std::to_string(data)}};
  }
};

template<>
struct ScalarDefaultsRO<UA_String> : ScalarDefaults<UA_String> {
  bool isWriteable() override { return false; };
};

template<>
struct ScalarDefaults<UA_Boolean> : AllRegisterDefaults {
  using AllRegisterDefaults::AllRegisterDefaults;
  static size_t nElementsPerChannel() { return 1; }
  virtual std::string path() = 0;

  // \ToDo: Is that needed for OPC UA?
  //  static constexpr auto capabilities = ScalarDefaults::capabilities.enableAsyncReadInconsistency();

  UA_Boolean* data;

  template<typename UserType>
  std::vector<std::vector<UserType>> getRemoteValue() {
    auto* variant = OPCUALauncher::threadedServer->server.getValue(path());
    data = (UA_Boolean*)variant->data;
    // convert to int
    int idata = *data;
    auto d = ChimeraTK::numericToUserType<UserType>(idata);
    UA_Variant_delete(variant);
    return {{d}};
  }

  void setRemoteValue() {
    std::vector<UA_Boolean> value;
    value.push_back(generateValue<int>().at(0).at(0));
    std::stringstream ss;
    ss << value.at(0);
    UA_LOG_DEBUG(&OPCUAServer::logger, UA_LOGCATEGORY_USERLAND, "Setting value:   %s", ss.str().c_str());
    OPCUALauncher::threadedServer->server.setValue(path(), value);
    std::this_thread::sleep_for(std::chrono::milliseconds(2 * publishingInterval));
  }

  template<typename UserType>
  std::vector<std::vector<UserType>> generateValue() {
    UserType increment(1);
    auto currentData = getRemoteValue<UserType>();
    if(currentData.at(0).at(0) > 0) {
      UserType data = currentData.at(0).at(0) - increment;
      return {{data}};
    }
    UserType data = currentData.at(0).at(0) + increment;
    return {{data}};
  }
};

template<>
struct ScalarDefaultsRO<UA_Boolean> : ScalarDefaults<UA_Boolean> {
  bool isWriteable() override { return false; };
};

template<typename UAType>
struct ArrayDefaults : AllRegisterDefaults {
  using AllRegisterDefaults::AllRegisterDefaults;
  size_t nElementsPerChannel() { return 5; }
  virtual std::string path() = 0;
  UAType* data;

  // \ToDo: Is that needed for OPC UA?
  //  static constexpr auto capabilities = ScalarDefaults::capabilities.enableAsyncReadInconsistency();

  template<typename UserType>
  std::vector<std::vector<UserType>> generateValue() {
    auto currentData = getRemoteValue<UserType>();
    UserType increment(3);
    std::vector<UserType> val;
    for(size_t i = 0; i < nElementsPerChannel(); ++i) {
      val.push_back(currentData.at(0).at(i) + (i + 1) * increment);
    }
    return {val};
  }

  template<typename UserType>
  std::vector<std::vector<UserType>> getRemoteValue() {
    auto variant = OPCUALauncher::threadedServer->server.getValue(path());
    data = (UAType*)variant->data;
    std::vector<UserType> values;
    for(size_t i = 0; i < nElementsPerChannel(); ++i) {
      auto value = ChimeraTK::numericToUserType<UserType>(data[i]);
      values.push_back(value);
    }
    UA_Variant_delete(variant);
    return {values};
  }

  void setRemoteValue() {
    std::vector<UAType> values;
    //\ToDo: Should this be UserType instead of UAType?
    auto v = generateValue<UAType>().at(0);
    std::stringstream ss;
    for(auto t : v) {
      values.push_back(t);
      ss << " " << t;
    }
    UA_LOG_DEBUG(&OPCUAServer::logger, UA_LOGCATEGORY_USERLAND, "Setting array:   %s", ss.str().c_str());
    OPCUALauncher::threadedServer->server.setValue(path(), values, nElementsPerChannel());
    std::this_thread::sleep_for(std::chrono::milliseconds(2 * publishingInterval));
  }
};

template<typename UAType>
struct ArrayDefaultsRO : ArrayDefaults<UAType> {
  bool isWriteable() override { return false; }
};

template<>
struct ArrayDefaults<UA_String> : AllRegisterDefaults {
  using AllRegisterDefaults::AllRegisterDefaults;
  static size_t nElementsPerChannel() { return 5; }
  virtual std::string path() = 0;
  UA_String* data;

  // \ToDo: Is that needed for OPC UA?
  //  static constexpr auto capabilities = ScalarDefaults::capabilities.enableAsyncReadInconsistency();

  template<typename UserType>
  std::vector<std::vector<UserType>> generateValue() {
    return generateValue(Identity<UserType>());
  }

  template<typename UserType>
  std::vector<std::vector<UserType>> getRemoteValue() {
    auto* variant = OPCUALauncher::threadedServer->server.getValue(path());
    data = (UA_String*)variant->data;
    std::vector<UserType> values;
    int tmpInt;
    for(size_t i = 0; i < nElementsPerChannel(); ++i) {
      std::stringstream ss(std::string((char*)data[i].data, data[i].length));
      ss >> tmpInt;
      auto value = ChimeraTK::numericToUserType<UserType>(tmpInt);
      values.push_back(value);
    }
    UA_Variant_delete(variant);
    return {values};
  }

  void setRemoteValue() {
    std::vector<UA_String> values;
    auto v = generateValue<std::string>().at(0);
    std::stringstream ss;
    for(auto& t : v) {
      values.push_back(UA_STRING((char*)t.c_str()));
      ss << " " << t;
    }
    UA_LOG_DEBUG(&OPCUAServer::logger, UA_LOGCATEGORY_USERLAND, "Setting array:   %s", ss.str().c_str());
    OPCUALauncher::threadedServer->server.setValue(path(), values, nElementsPerChannel());
    std::this_thread::sleep_for(std::chrono::milliseconds(2 * publishingInterval));
  }

 private:
  template<typename UserType>
  std::vector<std::vector<UserType>> generateValue(Identity<UserType>) {
    auto currentData = getRemoteValue<UserType>();
    UserType increment(3);
    std::vector<UserType> val;
    for(size_t i = 0; i < nElementsPerChannel(); ++i) {
      val.push_back(currentData.at(0).at(i) + (i + 1) * increment);
    }
    return {val};
  }

  std::vector<std::vector<std::string>> generateValue(Identity<std::string>) {
    auto currentData = getRemoteValue<int>();
    int increment(3);
    std::vector<std::string> val(nElementsPerChannel());
    for(size_t i = 0; i < nElementsPerChannel(); ++i) {
      val.at(i) = std::to_string(currentData.at(0).at(i) + (i + 1) * increment);
    }
    return {val};
  }
};

template<>
struct ArrayDefaultsRO<UA_String> : ArrayDefaults<UA_String> {
  bool isWriteable() override { return false; }
};

template<>
struct ArrayDefaults<UA_Boolean> : AllRegisterDefaults {
  using AllRegisterDefaults::AllRegisterDefaults;
  static size_t nElementsPerChannel() { return 5; }
  virtual std::string path() = 0;
  UA_Boolean* data;

  // \ToDo: Is that needed for OPC UA?
  //  static constexpr auto capabilities = ScalarDefaults::capabilities.enableAsyncReadInconsistency();

  template<typename UserType>
  std::vector<std::vector<UserType>> getRemoteValue() {
    auto* variant = OPCUALauncher::threadedServer->server.getValue(path());
    data = (UA_Boolean*)variant->data;
    // convert to int
    int idata;
    std::vector<UserType> d;
    for(size_t i = 0; i < nElementsPerChannel(); ++i) {
      idata = data[i];
      d.push_back(ChimeraTK::numericToUserType<UserType>(idata));
    }
    UA_Variant_delete(variant);
    return {d};
  }

  void setRemoteValue() {
    std::vector<UA_Boolean> values;
    auto v = generateValue<int>().at(0);
    std::stringstream ss;
    for(auto& t : v) {
      values.push_back(t);
      ss << " " << t;
    }
    UA_LOG_DEBUG(&OPCUAServer::logger, UA_LOGCATEGORY_USERLAND, "Setting array:   %s", ss.str().c_str());
    OPCUALauncher::threadedServer->server.setValue(path(), values, nElementsPerChannel());
    std::this_thread::sleep_for(std::chrono::milliseconds(2 * publishingInterval));
  }

  template<typename UserType>
  std::vector<std::vector<UserType>> generateValue() {
    auto currentData = getRemoteValue<UserType>();
    UserType increment(1);
    std::vector<UserType> val;
    for(size_t i = 0; i < nElementsPerChannel(); ++i) {
      if(currentData.at(0).at(i) > 0) {
        val.push_back(currentData.at(0).at(i) - increment);
      }
      else {
        val.push_back(currentData.at(0).at(i) + increment);
      }
    }
    return {val};
  }
};

template<>
struct ArrayDefaultsRO<UA_Boolean> : ArrayDefaults<UA_Boolean> {
  bool isWriteable() override { return false; }
};

struct RegSomeBool : ScalarDefaults<UA_Boolean> {
  std::string path() override { return "Dummy/scalar/bool"; }
  using minimumUserType = Boolean;
};

struct RegSomeInt64 : ScalarDefaults<UA_Int64> {
  std::string path() override { return "Dummy/scalar/int64"; }
  using minimumUserType = int64_t;
};

struct RegSomeUInt64 : ScalarDefaults<UA_UInt64> {
  std::string path() override { return "Dummy/scalar/uint64"; }
  using minimumUserType = uint64_t;
};

struct RegSomeInt32 : ScalarDefaults<UA_Int32> {
  std::string path() override { return "Dummy/scalar/int32"; }
  using minimumUserType = int32_t;
};

struct RegSomeUInt32 : ScalarDefaults<UA_UInt32> {
  std::string path() override { return "Dummy/scalar/uint32"; }
  using minimumUserType = uint32_t;
};

struct RegSomeInt16 : ScalarDefaults<UA_Int16> {
  std::string path() override { return "Dummy/scalar/int16"; }
  using minimumUserType = int16_t;
};

struct RegSomeUInt16 : ScalarDefaults<UA_UInt16> {
  std::string path() override { return "Dummy/scalar/uint16"; }
  using minimumUserType = uint16_t;
};

struct RegSomeFloat : ScalarDefaults<UA_Float> {
  std::string path() override { return "Dummy/scalar/float"; }
  using minimumUserType = float;
};

struct RegSomeDouble : ScalarDefaults<UA_Double> {
  std::string path() override { return "Dummy/scalar/double"; }
  using minimumUserType = double;
};

struct RegSomeString : ScalarDefaults<UA_String> {
  std::string path() override { return "Dummy/scalar/string"; }
  using minimumUserType = std::string;
};

struct RegSomeBoolArray : ArrayDefaults<UA_Boolean> {
  std::string path() override { return "Dummy/array/bool"; }
  using minimumUserType = Boolean;
};

struct RegSomeInt64Array : ArrayDefaults<UA_Int64> {
  std::string path() override { return "Dummy/array/int64"; }
  using minimumUserType = int64_t;
};

struct RegSomeUInt64Array : ArrayDefaults<UA_UInt64> {
  std::string path() override { return "Dummy/array/uint64"; }
  using minimumUserType = uint64_t;
};

struct RegSomeInt32Array : ArrayDefaults<UA_Int32> {
  std::string path() override { return "Dummy/array/int32"; }
  using minimumUserType = int32_t;
};

struct RegSomeUInt32Array : ArrayDefaults<UA_UInt32> {
  std::string path() override { return "Dummy/array/uint32"; }
  using minimumUserType = uint32_t;
};

struct RegSomeInt16Array : ArrayDefaults<UA_Int16> {
  std::string path() override { return "Dummy/array/int16"; }
  using minimumUserType = int16_t;
};

struct RegSomeUInt16Array : ArrayDefaults<UA_UInt16> {
  std::string path() override { return "Dummy/array/uint16"; }
  using minimumUserType = uint16_t;
};

struct RegSomeFloatArray : ArrayDefaults<UA_Float> {
  std::string path() override { return "Dummy/array/float"; }
  using minimumUserType = float;
};

struct RegSomeDoubleArray : ArrayDefaults<UA_Double> {
  std::string path() override { return "Dummy/array/double"; }
  using minimumUserType = double;
};

struct RegSomeStringArray : ArrayDefaults<UA_String> {
  std::string path() override { return "Dummy/array/string"; }
  using minimumUserType = std::string;
};

// read only part
struct RegSomeBoolRO : ScalarDefaultsRO<UA_Boolean> {
  std::string path() override { return "Dummy/scalar_ro/bool"; }
  using minimumUserType = Boolean;
};

struct RegSomeInt64RO : ScalarDefaultsRO<UA_Int64> {
  std::string path() override { return "Dummy/scalar_ro/int64"; }
  using minimumUserType = int64_t;
};

struct RegSomeUInt64RO : ScalarDefaultsRO<UA_UInt64> {
  std::string path() override { return "Dummy/scalar_ro/uint64"; }
  using minimumUserType = uint64_t;
};

struct RegSomeInt32RO : ScalarDefaultsRO<UA_Int32> {
  std::string path() override { return "Dummy/scalar_ro/int32"; }
  using minimumUserType = int32_t;
};

struct RegSomeUInt32RO : ScalarDefaultsRO<UA_UInt32> {
  std::string path() override { return "Dummy/scalar_ro/uint32"; }
  using minimumUserType = uint32_t;
};

struct RegSomeInt16RO : ScalarDefaultsRO<UA_Int16> {
  std::string path() override { return "Dummy/scalar_ro/int16"; }
  using minimumUserType = int16_t;
};

struct RegSomeUInt16RO : ScalarDefaultsRO<UA_UInt16> {
  std::string path() override { return "Dummy/scalar_ro/uint16"; }
  using minimumUserType = uint16_t;
};

struct RegSomeFloatRO : ScalarDefaultsRO<UA_Float> {
  std::string path() override { return "Dummy/scalar_ro/float"; }
  using minimumUserType = float;
};

struct RegSomeDoubleRO : ScalarDefaultsRO<UA_Double> {
  std::string path() override { return "Dummy/scalar_ro/double"; }
  using minimumUserType = double;
};

struct RegSomeStringRO : ScalarDefaultsRO<UA_String> {
  std::string path() override { return "Dummy/scalar_ro/string"; }
  using minimumUserType = std::string;
};

struct RegSomeBoolArrayRO : ArrayDefaultsRO<UA_Boolean> {
  std::string path() override { return "Dummy/array_ro/bool"; }
  using minimumUserType = Boolean;
};

struct RegSomeInt64ArrayRO : ArrayDefaultsRO<UA_Int64> {
  std::string path() override { return "Dummy/array_ro/int64"; }
  using minimumUserType = int64_t;
};

struct RegSomeUInt64ArrayRO : ArrayDefaultsRO<UA_UInt64> {
  std::string path() override { return "Dummy/array_ro/uint64"; }
  using minimumUserType = uint64_t;
};

struct RegSomeInt32ArrayRO : ArrayDefaultsRO<UA_Int32> {
  std::string path() override { return "Dummy/array_ro/int32"; }
  using minimumUserType = int32_t;
};

struct RegSomeUInt32ArrayRO : ArrayDefaultsRO<UA_UInt32> {
  std::string path() override { return "Dummy/array_ro/uint32"; }
  using minimumUserType = uint32_t;
};

struct RegSomeInt16ArrayRO : ArrayDefaultsRO<UA_Int16> {
  std::string path() override { return "Dummy/array_ro/int16"; }
  using minimumUserType = int16_t;
};

struct RegSomeUInt16ArrayRO : ArrayDefaultsRO<UA_UInt16> {
  std::string path() override { return "Dummy/array_ro/uint16"; }
  using minimumUserType = uint16_t;
};

struct RegSomeFloatArrayRO : ArrayDefaultsRO<UA_Float> {
  std::string path() override { return "Dummy/array_ro/float"; }
  using minimumUserType = float;
};

struct RegSomeDoubleArrayRO : ArrayDefaultsRO<UA_Double> {
  std::string path() override { return "Dummy/array_ro/double"; }
  using minimumUserType = double;
};

struct RegSomeStringArrayRO : ArrayDefaultsRO<UA_String> {
  std::string path() override { return "Dummy/array_ro/string"; }
  using minimumUserType = std::string;
};

// use test fixture suite to have access to the fixture class members
BOOST_FIXTURE_TEST_SUITE(s, OPCUALauncher)
BOOST_AUTO_TEST_CASE(unifiedBackendTest) {
  auto ubt = ChimeraTK::UnifiedBackendTest<>()
                 .addRegister<RegSomeInt16>()
                 .addRegister<RegSomeBool>()
                 .addRegister<RegSomeUInt16>()
                 .addRegister<RegSomeInt32>()
                 .addRegister<RegSomeUInt32>()
                 .addRegister<RegSomeInt64>()
                 .addRegister<RegSomeUInt64>()
                 .addRegister<RegSomeFloat>()
                 .addRegister<RegSomeDouble>()
                 .addRegister<RegSomeString>()
                 .addRegister<RegSomeBoolArray>()
                 .addRegister<RegSomeInt16Array>()
                 .addRegister<RegSomeUInt16Array>()
                 .addRegister<RegSomeInt32Array>()
                 .addRegister<RegSomeUInt32Array>()
                 .addRegister<RegSomeInt64Array>()
                 .addRegister<RegSomeUInt64Array>()
                 .addRegister<RegSomeFloatArray>()
                 .addRegister<RegSomeDoubleArray>()
                 .addRegister<RegSomeStringArray>()
                 .addRegister<RegSomeBoolRO>()
                 .addRegister<RegSomeInt16RO>()
                 .addRegister<RegSomeUInt16RO>()
                 .addRegister<RegSomeInt32RO>()
                 .addRegister<RegSomeUInt32RO>()
                 .addRegister<RegSomeInt64RO>()
                 .addRegister<RegSomeUInt64RO>()
                 .addRegister<RegSomeFloatRO>()
                 .addRegister<RegSomeDoubleRO>()
                 .addRegister<RegSomeStringRO>()
                 .addRegister<RegSomeBoolArrayRO>()
                 .addRegister<RegSomeInt16ArrayRO>()
                 .addRegister<RegSomeUInt16ArrayRO>()
                 .addRegister<RegSomeInt32ArrayRO>()
                 .addRegister<RegSomeUInt32ArrayRO>()
                 .addRegister<RegSomeInt64ArrayRO>()
                 .addRegister<RegSomeUInt64ArrayRO>()
                 .addRegister<RegSomeFloatArrayRO>()
                 .addRegister<RegSomeDoubleArrayRO>()
                 .addRegister<RegSomeStringArrayRO>();
  // wait for the server to come up
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::stringstream ss;
  ss << "(" << path << "?port=" << port << "&publishingInterval=" << publishingInterval << "&connectionTimeout=10000"
     << "&logLevel=error"
     << ")";
  // server side logging severity level can be changed in DummyServer.h -> testServerLogLevel
  ubt.runTests(ss.str());
}

BOOST_AUTO_TEST_SUITE_END()
