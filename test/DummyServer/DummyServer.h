/*
 * DummyServer.h
 *
 *  Created on: Dec 11, 2020
 *      Author: Klaus Zenker (HZDR)
 */

#pragma once

#include "open62541.h"

#include <thread>
#include <chrono>
#include <mutex>

#include <boost/fusion/container/map.hpp>

namespace fusion = boost::fusion;

typedef fusion::map<
    fusion::pair<UA_Int16, std::pair<std::string,UA_DataType> >
  , fusion::pair<UA_UInt16, std::pair<std::string,UA_DataType> >
  , fusion::pair<UA_Int32, std::pair<std::string,UA_DataType> >
  , fusion::pair<UA_UInt32, std::pair<std::string,UA_DataType> >
  , fusion::pair<UA_Int64, std::pair<std::string,UA_DataType> >
  , fusion::pair<UA_UInt64, std::pair<std::string,UA_DataType> >
  , fusion::pair<UA_Double, std::pair<std::string,UA_DataType> >
  , fusion::pair<UA_Float, std::pair<std::string,UA_DataType> >
  , fusion::pair<UA_String, std::pair<std::string,UA_DataType> >
  , fusion::pair<UA_SByte, std::pair<std::string,UA_DataType> >
  , fusion::pair<UA_Byte, std::pair<std::string,UA_DataType> > > TypeMapWithName;

extern TypeMapWithName dummyMap;

enum ServerState{On, Off};

struct OPCUAServer{

  OPCUAServer();

  ~OPCUAServer();

  UA_ServerConfig _config;
  UA_Server *_server;
  UA_ServerNetworkLayer _nl;
  uint _port;

  UA_Boolean running{true};

  std::mutex _mux;

  void start();

  void lock(){_mux.lock();}
  void unlock(){_mux.unlock();}

  uint getPort(){return _port;}

  /*
   * Stop the server by setting running to false
   * This can be done by a signal handler or by setting running to false from outside -
   * so no interaction with the server is needed...
   */
  void stop(){
    running = false;
  }

  void addFolder(std::string name, UA_NodeId parent);

  void addVariables();

  template <typename UAType>
  void setValue(std::string nodeName, const UAType &t);

  UA_Variant* getValue(std::string nodeName);

};

template <typename UAType>
void OPCUAServer::setValue(std::string nodeName, const UAType &t){
  UA_Variant* data = UA_Variant_new();
  UA_Variant_setScalarCopy(data,&t,&fusion::at_key<UAType>(dummyMap).second);
  UA_Server_writeValue(_server, UA_NODEID_STRING(1, &nodeName[0]), *data);
  UA_Variant_delete(data);
  // in the test the publish interval is set 100ms so after 150ms the handler should have been called/the server should have published the result.
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
}

class ThreadedOPCUAServer {
public:
  std::thread _serverThread;

  ThreadedOPCUAServer(){ }
  ~ThreadedOPCUAServer();

  void start();

  /**
   * Check if the server has expected state.
   */
  bool checkConnection(const ServerState &state = ServerState::On);

  OPCUAServer _server;
};