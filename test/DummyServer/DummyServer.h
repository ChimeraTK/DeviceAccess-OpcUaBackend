// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
/*
 * DummyServer.h
 *
 *  Created on: Dec 11, 2020
 *      Author: Klaus Zenker (HZDR)
 */

#include <open62541/plugin/log.h>
#include <open62541/plugin/log_stdout.h>
#include <open62541/plugin/network.h>
#include <open62541/server.h>
#include <open62541/types.h>

#include <boost/fusion/container/map.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

namespace fusion = boost::fusion;

typedef fusion::map<fusion::pair<UA_Int16, std::pair<std::string, UA_UInt16>>,
    fusion::pair<UA_UInt16, std::pair<std::string, UA_UInt16>>,
    fusion::pair<UA_Int32, std::pair<std::string, UA_UInt16>>,
    fusion::pair<UA_UInt32, std::pair<std::string, UA_UInt16>>,
    fusion::pair<UA_Int64, std::pair<std::string, UA_UInt16>>,
    fusion::pair<UA_UInt64, std::pair<std::string, UA_UInt16>>,
    fusion::pair<UA_Double, std::pair<std::string, UA_UInt16>>,
    fusion::pair<UA_Float, std::pair<std::string, UA_UInt16>>,
    fusion::pair<UA_String, std::pair<std::string, UA_UInt16>>,
    fusion::pair<UA_SByte, std::pair<std::string, UA_UInt16>>, fusion::pair<UA_Byte, std::pair<std::string, UA_UInt16>>,
    fusion::pair<UA_Boolean, std::pair<std::string, UA_UInt16>>>
    TypeMapWithName;

extern TypeMapWithName dummyMap;

enum ServerState { On, Off };

// The publishing and sampling interval used in the tests
static constexpr int publishingInterval{200}; // ms
static constexpr UA_LogLevel testServerLogLevel{UA_LOGLEVEL_ERROR};

struct OPCUAServer {
  OPCUAServer();

  ~OPCUAServer();

  UA_Server* _server;

  uint _port{0};

  bool _configured{false};

  std::atomic<UA_Boolean> running{true};

  std::mutex _mux;

  static UA_Logger logger;

  void start();

  void lock() { _mux.lock(); }
  void unlock() { _mux.unlock(); }

  uint getPort() { return _port; }

  /*
   * Stop the server by setting running to false
   * This can be done by a signal handler or by setting running to false from outside -
   * so no interaction with the server is needed...
   */
  void stop() { running = false; }

  void addFolder(std::string name, UA_NodeId parent);

  void addVariables();

  template<typename UAType>
  void setValue(std::string nodeName, const std::vector<UAType>& t, const size_t& length = 1);

  // This is needed because std::vector<bool> is a special vector in the stl!
  void setValue(std::string nodeName, const std::vector<UA_Boolean>& t, const size_t& length = 1);

  UA_Variant* getValue(std::string nodeName);
};

template<typename UAType>
void OPCUAServer::setValue(std::string nodeName, const std::vector<UAType>& t, const size_t& length) {
  UA_Variant* data = UA_Variant_new();
  if(t.size() == 1) {
    UA_Variant_setScalarCopy(data, &t[0], &UA_TYPES[fusion::at_key<UAType>(dummyMap).second]);
  }
  else {
    UA_Variant_setArrayCopy(data, &t[0], length, &UA_TYPES[fusion::at_key<UAType>(dummyMap).second]);
  }
  UA_Server_writeValue(_server, UA_NODEID_STRING(1, &nodeName[0]), *data);
  UA_Variant_delete(data);
  // in the test the publish interval is set 100ms so after 150ms the handler should have been called/the server should
  // have published the result. Nevertheless problems were observed so use 300ms.
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

class ThreadedOPCUAServer {
 public:
  std::thread _serverThread;

  ThreadedOPCUAServer();
  ~ThreadedOPCUAServer();

  void start();

  /**
   * Check if the server has expected state.
   */
  bool checkConnection(const ServerState& state = ServerState::On);

  OPCUAServer _server;

 private:
  UA_Client* _client;
};
