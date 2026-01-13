// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * DummyServer.cc
 *
 *  Created on: Dec 11, 2020
 *      Author: Klaus Zenker (HZDR)
 */

#include "DummyServer.h"

#include <open62541/client_config_default.h>
#include <open62541/plugin/log_stdout.h>
#include <open62541/server_config_default.h>

#include <boost/fusion/include/at_key.hpp>
#include <boost/fusion/include/for_each.hpp>

#include <memory>
#include <random>
#include <stdexcept>
#include <vector>

namespace fusion = boost::fusion;

UA_Logger OPCUAServer::logger;

TypeMapWithName dummyMap(fusion::make_pair<UA_Int16>(std::make_pair("int16", UA_TYPES_INT16)),
    fusion::make_pair<UA_UInt16>(std::make_pair("uint16", UA_TYPES_UINT16)),
    fusion::make_pair<UA_Int32>(std::make_pair("int32", UA_TYPES_INT32)),
    fusion::make_pair<UA_UInt32>(std::make_pair("uint32", UA_TYPES_UINT32)),
    fusion::make_pair<UA_Int64>(std::make_pair("int64", UA_TYPES_INT64)),
    fusion::make_pair<UA_UInt64>(std::make_pair("uint64", UA_TYPES_UINT64)),
    fusion::make_pair<UA_Double>(std::make_pair("double", UA_TYPES_DOUBLE)),
    fusion::make_pair<UA_Float>(std::make_pair("float", UA_TYPES_FLOAT)),
    fusion::make_pair<UA_String>(std::make_pair("string", UA_TYPES_STRING)),
    fusion::make_pair<UA_SByte>(std::make_pair("int8", UA_TYPES_SBYTE)),
    fusion::make_pair<UA_Byte>(std::make_pair("uint8", UA_TYPES_BYTE)),
    fusion::make_pair<UA_Boolean>(std::make_pair("bool", UA_TYPES_BOOLEAN)));

struct VariableAttacher {
  UA_NodeId parent;
  UA_Server* server;
  bool isArray;
  bool readOnly;

  VariableAttacher(UA_NodeId parent, UA_Server* server, bool isArray = false, bool readOnly = false)
  : parent(parent), server(server), isArray(isArray), readOnly(readOnly) {}

  ~VariableAttacher() { UA_NodeId_clear(&parent); }

  template<typename PAIR>
  void operator()(PAIR& pair) const {
    // typedef typename PAIR::first_type UAType;
    using UAType = typename PAIR::first_type;
    auto mypair = pair.second;
    /* Define the attribute of the myInteger variable node */
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.dataType = UA_TYPES[mypair.second].typeId;
    UAType data = 42;
    std::vector<UAType> v = {42, 42, 42, 42, 42};
    std::string name = std::string((char*)parent.identifier.string.data, parent.identifier.string.length);
    name = name + "/" + mypair.first;
    //    UA_DataType t = mypair.second;
    if(isArray) {
      // passing mypair.second directly does not work!
      //      UA_Variant_setArray(&attr.value, &v[0], 5, &UA_TYPES[mypair.second.typeId.identifier.numeric]);
      UA_Variant_setArray(&attr.value, &v[0], 5, &UA_TYPES[mypair.second]);
      attr.valueRank = 1;
      UA_UInt32 myArrayDimensions[1] = {5};
      attr.arrayDimensions = myArrayDimensions;
      attr.arrayDimensionsSize = 1;
    }
    else {
      // passing mypair.second directly does not work!
      //      UA_Variant_setScalar(&attr.value, &data, &UA_TYPES[mypair.second.typeId.identifier.numeric]);
      UA_Variant_setScalar(&attr.value, &data, &UA_TYPES[mypair.second]);
      attr.valueRank = -1;
    }
    UA_LocalizedText locText = UA_LOCALIZEDTEXT_ALLOC("en_US", &mypair.first[0]);
    attr.description = locText;
    attr.displayName = locText;
    attr.userWriteMask = UA_ACCESSLEVELMASK_WRITE;
    attr.writeMask = UA_ACCESSLEVELMASK_WRITE;
    if(readOnly) {
      attr.accessLevel = UA_ACCESSLEVELMASK_READ;
      attr.userAccessLevel = UA_ACCESSLEVELMASK_READ;
    }
    else {
      attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
      attr.userAccessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
    }

    /* Add the variable node to the information model */
    UA_NodeId nodeId = UA_NODEID_STRING(1, &name[0]);
    UA_QualifiedName nodeName = UA_QUALIFIEDNAME(1, &mypair.first[0]);
    UA_Server_addVariableNode(server, nodeId, parent, UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), nodeName,
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);
    UA_LOG_DEBUG(&OPCUAServer::logger, UA_LOGCATEGORY_USERLAND, "Trying to add node:  %s with name: %s", name.c_str(),
        mypair.first.c_str());
    UA_LocalizedText_clear(&locText);
    for(size_t i = 0; i < 5; ++i) {
      //      UA_clear(&v[i], &UA_TYPES[mypair.second.typeId.identifier.numeric]);
      UA_clear(&v[i], &UA_TYPES[mypair.second]);
    }
  }

  // void operator()(fusion::pair<UA_String, std::pair<std::string, UA_DataType>>& pair) const {
  void operator()(fusion::pair<UA_String, std::pair<std::string, UA_UInt16>>& pair) const {
    //      auto mypair = fusion::at_key<UA_String>(m);
    auto mypair = pair.second;
    /* Define the attribute of the myInteger variable node */
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    std::string strData = "42";
    std::vector<UA_String> v = {UA_String_fromChars("42"), UA_String_fromChars("42"), UA_String_fromChars("42"),
        UA_String_fromChars("42"), UA_String_fromChars("42")};
    UA_String data = UA_STRING((char*)strData.c_str());
    std::string name = std::string((char*)parent.identifier.string.data, parent.identifier.string.length);
    name = name + "/" + mypair.first;
    if(isArray) {
      // passing mypair.second directly does not work!
      UA_Variant_setArray(&attr.value, &v[0], 5, &UA_TYPES[UA_TYPES_STRING]);
      attr.valueRank = 1;
      UA_UInt32 myArrayDimensions[1] = {5};
      attr.arrayDimensions = myArrayDimensions;
      attr.arrayDimensionsSize = 1;
    }
    else {
      // passing mypair.second directly does not work!
      UA_Variant_setScalar(&attr.value, &data, &UA_TYPES[UA_TYPES_STRING]);
      attr.valueRank = -1;
    }
    UA_LocalizedText locText = UA_LOCALIZEDTEXT_ALLOC("en_US", &mypair.first[0]);
    attr.description = locText;
    attr.displayName = locText;
    attr.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
    attr.userWriteMask = UA_ACCESSLEVELMASK_WRITE;
    attr.writeMask = UA_ACCESSLEVELMASK_WRITE;
    if(readOnly) {
      attr.accessLevel = UA_ACCESSLEVELMASK_READ;
      attr.userAccessLevel = UA_ACCESSLEVELMASK_READ;
    }
    else {
      attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
      attr.userAccessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
      ;
    }

    /* Add the variable node to the information model */
    UA_NodeId myNodeId = UA_NODEID_STRING(1, &name[0]);
    UA_QualifiedName myName = UA_QUALIFIEDNAME(1, &mypair.first[0]);
    UA_Server_addVariableNode(server, myNodeId, parent, UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), myName,
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);
    UA_LOG_DEBUG(&OPCUAServer::logger, UA_LOGCATEGORY_USERLAND, "Trying to add node:  %s with name: %s", name.c_str(),
        mypair.first.c_str());
    UA_LocalizedText_clear(&locText);
    for(size_t i = 0; i < 5; ++i) {
      UA_String_clear(&v[i]);
    }
  }

  //  void operator()(fusion::pair<UA_Boolean, std::pair<std::string, UA_DataType>>& pair) const {
  void operator()(fusion::pair<UA_Boolean, std::pair<std::string, UA_UInt16>>& pair) const {
    //      auto mypair = fusion::at_key<UA_String>(m);
    auto mypair = pair.second;
    /* Define the attribute of the myInteger variable node */
    UA_VariableAttributes attr;
    UA_VariableAttributes_init(&attr);
    UA_Boolean data = true;
    bool p[5] = {true, true, true, true, true};
    std::string name = std::string((char*)parent.identifier.string.data, parent.identifier.string.length);
    name = name + "/" + mypair.first;
    if(isArray) {
      // passing mypair.second directly does not work!
      //      UA_Variant_setArray(&attr.value, &boolVector.data(), 5,  &UA_TYPES[mypair.second.typeIndex]);
      UA_Variant_setArray(&attr.value, &p[0], 5, &UA_TYPES[UA_TYPES_BOOLEAN]);
      attr.valueRank = 1;
      UA_UInt32 myArrayDimensions[1] = {5};
      attr.arrayDimensions = myArrayDimensions;
      attr.arrayDimensionsSize = 1;
    }
    else {
      // passing mypair.second directly does not work!
      UA_Variant_setScalar(&attr.value, &data, &UA_TYPES[UA_TYPES_BOOLEAN]);
      attr.valueRank = -1;
    }

    UA_LocalizedText locText = UA_LOCALIZEDTEXT_ALLOC("en_US", &mypair.first[0]);
    attr.description = locText;
    attr.displayName = locText;
    attr.dataType = UA_TYPES[UA_TYPES_BOOLEAN].typeId;
    attr.userWriteMask = UA_ACCESSLEVELMASK_WRITE;
    attr.writeMask = UA_ACCESSLEVELMASK_WRITE;
    if(readOnly) {
      attr.accessLevel = UA_ACCESSLEVELMASK_READ;
      attr.userAccessLevel = UA_ACCESSLEVELMASK_READ;
    }
    else {
      attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
      attr.userAccessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
    }

    /* Add the variable node to the information model */
    UA_NodeId myNodeId = UA_NODEID_STRING(1, &name[0]);
    UA_QualifiedName myName = UA_QUALIFIEDNAME(1, &mypair.first[0]);
    UA_Server_addVariableNode(server, myNodeId, parent, UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), myName,
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);
    UA_LOG_DEBUG(&OPCUAServer::logger, UA_LOGCATEGORY_USERLAND, "Trying to add node:  %s with name: %s", name.c_str(),
        mypair.first.c_str());
    UA_LocalizedText_clear(&locText);
  }
};

OPCUAServer::OPCUAServer() : server(nullptr) {
  /**
   *  A single random port will be used for all tests.
   *  This protects against problems when running the unit tests on the same machine multiple times.
   */
  std::random_device rd;
  std::uniform_int_distribution<uint> dist(20000, 22000);
  port = dist(rd);
  configured = false;
  OPCUAServer::logger = UA_Log_Stdout_withLevel(testServerLogLevel);
}

OPCUAServer::~OPCUAServer() {
  UA_Server_delete(server);
  UA_LOG_INFO(&OPCUAServer::logger, UA_LOGCATEGORY_USERLAND, "Destroyed OPCUAServer object.");
}

static UA_INLINE UA_DurationRange getDuration(UA_Duration min, UA_Duration max) {
  UA_DurationRange range = {min, max};
  return range;
}

void OPCUAServer::start() {
  lock();
  // delete server if it was used already before
  if(configured) {
    UA_LOG_INFO(&OPCUAServer::logger, UA_LOGCATEGORY_USERLAND, "Destroying OPCUAServer object.");
    UA_Server_delete(server);
  }
  // set up the server
  auto config = UA_ServerConfig();
  config.logging = &OPCUAServer::logger;
  UA_ServerConfig_setMinimal(&config, port, NULL);
  config.publishingIntervalLimits = getDuration(publishingInterval, 3600.0 * 1000.0);
  config.samplingIntervalLimits = getDuration(publishingInterval, 24.0 * 3600.0 * 1000.0);
  server = UA_Server_newWithConfig(&config);

  addVariables();
  configured = true;
  running = true;
  // run the server
  /**
   *  This is a version without the option to lock in between
   */
  //  UA_Server_run(_server, &running);
  /**
   * Here we use UA_Server_run_iterate to allow to lock in between
   */
  UA_Server_run_startup(server);
  // make sure to kepp the lock until UA_Server_run_iterate is called once to emit initial values
  bool isFirstLock = true;
  while(running) {
    if(!isFirstLock) {
      lock();
    }
    else {
      isFirstLock = false;
    }
    auto nextUpdate = UA_Server_run_iterate(server, true);
    //    usleep(nextUpdate);
    unlock();
    usleep(1000);
  }
  UA_LOG_INFO(&OPCUAServer::logger, UA_LOGCATEGORY_USERLAND, "Finished iterate loop in the server.");

  lock();
  UA_Server_run_shutdown(server);
  unlock();
  UA_LOG_INFO(&OPCUAServer::logger, UA_LOGCATEGORY_USERLAND, "Finished running the server.");
}

void OPCUAServer::addFolder(std::string name, UA_NodeId parent) {
  std::string displayName = name.substr(name.find_last_of('/') + 1, name.size());
  UA_ObjectAttributes attrObj = UA_ObjectAttributes_default;
  attrObj.description = UA_LOCALIZEDTEXT_ALLOC("en_US", &displayName[0]);
  attrObj.displayName = UA_LOCALIZEDTEXT_ALLOC("en_US", &displayName[0]);
  UA_QualifiedName qname = UA_QUALIFIEDNAME(1, &displayName[0]);
  UA_NodeId objNode = UA_NODEID_STRING(1, &name[0]);
  UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
  UA_Server_addObjectNode(server, objNode, parent, UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char*)name.c_str()), UA_NODEID_NUMERIC(0, UA_NS0ID_FOLDERTYPE), attrObj, NULL, NULL);
  UA_ObjectAttributes_clear(&attrObj);
}

void OPCUAServer::addVariables() {
  /* Add object node */
  // Create our toplevel instance
  UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
  oAttr.displayName = UA_LOCALIZEDTEXT_ALLOC("en_US", "Dummy");
  oAttr.description = UA_LOCALIZEDTEXT_ALLOC("en_US", "Dummy");
  UA_NodeId id = UA_NODEID("ns=1;s=Dummy");
  UA_QualifiedName qName = UA_QUALIFIEDNAME_ALLOC(1, "Dummy");
  UA_Server_addObjectNode(server, id, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), qName, UA_NODEID_NUMERIC(0, UA_NS0ID_FOLDERTYPE), oAttr, NULL, NULL);
  UA_QualifiedName_clear(&qName);
  addFolder("Dummy/scalar", id);
  boost::fusion::for_each(dummyMap, VariableAttacher(UA_NODEID("ns=1;s=Dummy/scalar"), server, false, false));
  addFolder("Dummy/array", id);
  boost::fusion::for_each(dummyMap, VariableAttacher(UA_NODEID("ns=1;s=Dummy/array"), server, true, false));
  addFolder("Dummy/scalar_ro", id);
  boost::fusion::for_each(dummyMap, VariableAttacher(UA_NODEID("ns=1;s=Dummy/scalar_ro"), server, false, true));
  addFolder("Dummy/array_ro", id);
  boost::fusion::for_each(dummyMap, VariableAttacher(UA_NODEID("ns=1;s=Dummy/array_ro"), server, true, true));
  UA_NodeId_clear(&id);
  UA_ObjectAttributes_clear(&oAttr);
}

UA_Variant* OPCUAServer::getValue(std::string nodeName) {
  UA_Variant* data = UA_Variant_new();
  UA_Server_readValue(server, UA_NODEID_STRING(1, &nodeName[0]), data);
  return data;
}

void OPCUAServer::setValue(std::string nodeName, const std::vector<UA_Boolean>& t, const size_t& length) {
  UA_Variant* data = UA_Variant_new();
  if(t.size() > 100) {
    throw std::runtime_error("Vector size is too big");
  }
  bool p[100];
  for(size_t i = 0; i < 100; ++i) {
    p[i] = t[i];
  }

  if(t.size() == 1) {
    UA_Variant_setScalarCopy(data, &p[0], &UA_TYPES[UA_TYPES_BOOLEAN]);
  }
  else {
    UA_Variant_setArrayCopy(data, &p[0], length, &UA_TYPES[UA_TYPES_BOOLEAN]);
  }
  lock();
  UA_Server_writeValue(server, UA_NODEID_STRING(1, &nodeName[0]), *data);
  unlock();
  UA_Variant_delete(data);
  // In the test new data is set in a sequence. Since the sampling interval is set equal to the publishing interval we
  // have to wait at least one sampling interval here
  std::this_thread::sleep_for(std::chrono::milliseconds(2 * publishingInterval));
}

void ThreadedOPCUAServer::start() {
  if(serverThread.joinable()) {
    serverThread.join();
  }
  serverThread = std::thread{&OPCUAServer::start, &server};
  if(!checkConnection(ServerState::On)) {
    throw std::runtime_error("Failed to connect to the test server!");
  }
  UA_LOG_DEBUG(&OPCUAServer::logger, UA_LOGCATEGORY_USERLAND, "Test server is set up and running.");
}

ThreadedOPCUAServer::ThreadedOPCUAServer()
: _connection(std::make_unique<ChimeraTK::OPCUAConnection>(
      "", "", "", publishingInterval, 5000, testServerLogLevel, "", "", true, "", "")) {}

ThreadedOPCUAServer::~ThreadedOPCUAServer() {
  server.stop();
  serverThread.join();
}

bool ThreadedOPCUAServer::checkConnection(const ServerState& state) {
  /** Connect **/
  UA_StatusCode retval = UA_STATUSCODE_GOODINITIATEFAULTSTATE;
  std::string serverAddress("opc.tcp://localhost:" + std::to_string(server.port));
  uint time = 0;
  while(retval != UA_STATUSCODE_GOOD) {
    retval = UA_Client_connect(_connection->client.get(), serverAddress.c_str());
    if(state == ServerState::On) {
      if(retval == UA_STATUSCODE_GOOD) {
        break;
      }
    }
    else {
      if(retval != UA_STATUSCODE_GOOD) {
        break;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(publishingInterval));

    time++;
    if(time == 1000 / publishingInterval) {
      // break after 1s - server should be up now!
      return false;
    }
  }
  _connection->close();
  return true;
}
