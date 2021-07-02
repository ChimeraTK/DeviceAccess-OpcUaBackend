/*
 * DummyServer.cc
 *
 *  Created on: Dec 11, 2020
 *      Author: Klaus Zenker (HZDR)
 */

#include <random>
#include <vector>


#include <open62541/plugin/log_stdout.h>
#include <open62541/server_config_default.h>
#include <open62541/network_tcp.h>
#include <open62541/client_config_default.h>

#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/include/at_key.hpp>

#include "DummyServer.h"

namespace fusion = boost::fusion;

TypeMapWithName dummyMap(
    fusion::make_pair<UA_Int16>(std::make_pair("int16",UA_TYPES[UA_TYPES_INT16])),
    fusion::make_pair<UA_UInt16>(std::make_pair("uint16",UA_TYPES[UA_TYPES_UINT16])),
    fusion::make_pair<UA_Int32>(std::make_pair("int32",UA_TYPES[UA_TYPES_INT32])),
    fusion::make_pair<UA_UInt32>(std::make_pair("uint32",UA_TYPES[UA_TYPES_UINT32])),
    fusion::make_pair<UA_Int64>(std::make_pair("int64",UA_TYPES[UA_TYPES_INT64])),
    fusion::make_pair<UA_UInt64>(std::make_pair("uint64",UA_TYPES[UA_TYPES_UINT64])),
    fusion::make_pair<UA_Double>(std::make_pair("double",UA_TYPES[UA_TYPES_DOUBLE])),
    fusion::make_pair<UA_Float>(std::make_pair("float",UA_TYPES[UA_TYPES_FLOAT])),
    fusion::make_pair<UA_String>(std::make_pair("string",UA_TYPES[UA_TYPES_STRING])),
    fusion::make_pair<UA_SByte>(std::make_pair("int8",UA_TYPES[UA_TYPES_SBYTE])),
    fusion::make_pair<UA_Byte>(std::make_pair("uint8",UA_TYPES[UA_TYPES_BYTE])));



struct VariableAttacher{
  UA_NodeId _parent;
  UA_Server* _server;
  bool _isArray;
  bool _readOnly;


  VariableAttacher(UA_NodeId parent, UA_Server* server, bool isArray = false, bool readOnly = false):
  _parent(parent), _server(server), _isArray(isArray), _readOnly(readOnly){}

  template<typename PAIR>
  void operator()(PAIR& pair) const{
    typedef typename PAIR::first_type UAType;
    auto mypair = pair.second;
    /* Define the attribute of the myInteger variable node */
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    UAType data = 42;
    std::vector<UAType> v = {42,42,42,42,42};
    std::string name = std::string((char*)_parent.identifier.string.data, _parent.identifier.string.length);
    name = name + "/" + mypair.first;
    if (_isArray){
      // passing mypair.second directly does not work!
      UA_Variant_setArray(&attr.value, &v[0], 5,  &UA_TYPES[mypair.second.typeIndex]);
      attr.valueRank = 1;
      UA_UInt32 myArrayDimensions[1] = {5};
      attr.arrayDimensions = myArrayDimensions;
      attr.arrayDimensionsSize = 1;

    } else {
      // passing mypair.second directly does not work!
      UA_Variant_setScalar(&attr.value, &data, &UA_TYPES[mypair.second.typeIndex]);
      attr.valueRank = -1;

    }
    attr.description = UA_LOCALIZEDTEXT_ALLOC("en_US",&mypair.first[0]);
    attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en_US",&mypair.first[0]);
    attr.dataType = mypair.second.typeId;
    attr.userWriteMask = UA_ACCESSLEVELMASK_WRITE;
    attr.writeMask = UA_ACCESSLEVELMASK_WRITE;
    if(_readOnly){
      attr.accessLevel = UA_ACCESSLEVELMASK_READ;
      attr.userAccessLevel = UA_ACCESSLEVELMASK_READ;
    } else {
      attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
      attr.userAccessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
    }

    /* Add the variable node to the information model */
    UA_NodeId nodeId = UA_NODEID_STRING(1, &name[0]);
    UA_QualifiedName nodeName = UA_QUALIFIEDNAME(1, &mypair.first[0]);
    UA_Server_addVariableNode(_server, nodeId, _parent,
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), nodeName,
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                      "Trying to add node:  %s with name: %s" , name.c_str(), mypair.first.c_str());
  }

  void operator()(fusion::pair<UA_String, std::pair<std::string,UA_DataType> >& pair) const{
//      auto mypair = fusion::at_key<UA_String>(m);
      auto mypair = pair.second;
      /* Define the attribute of the myInteger variable node */
      UA_VariableAttributes attr = UA_VariableAttributes_default;
      std::string strData = "42";
      std::vector<UA_String> v = {UA_String_fromChars("42"), UA_String_fromChars("42"),UA_String_fromChars("42"),UA_String_fromChars("42"),UA_String_fromChars("42")};
      UA_String data = UA_STRING((char*)strData.c_str());
      std::string name = std::string((char*)_parent.identifier.string.data, _parent.identifier.string.length);
      name = name + "/" + mypair.first;
      if (_isArray){
        // passing mypair.second directly does not work!
        UA_Variant_setArray(&attr.value, &v[0], 5,  &UA_TYPES[mypair.second.typeIndex]);
        attr.valueRank = 1;
        UA_UInt32 myArrayDimensions[1] = {5};
        attr.arrayDimensions = myArrayDimensions;
        attr.arrayDimensionsSize = 1;
      } else {
        // passing mypair.second directly does not work!
        UA_Variant_setScalar(&attr.value, &data, &UA_TYPES[mypair.second.typeIndex]);
        attr.valueRank = -1;
      }

      attr.description = UA_LOCALIZEDTEXT_ALLOC("en_US",&mypair.first[0]);
      attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en_US",&mypair.first[0]);
      attr.dataType = mypair.second.typeId;
      attr.userWriteMask = UA_ACCESSLEVELMASK_WRITE;
      attr.writeMask = UA_ACCESSLEVELMASK_WRITE;
      if(_readOnly){
        attr.accessLevel = UA_ACCESSLEVELMASK_READ;
        attr.userAccessLevel = UA_ACCESSLEVELMASK_READ;
      } else {
        attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
        attr.userAccessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;;
      }

      /* Add the variable node to the information model */
      UA_NodeId myNodeId = UA_NODEID_STRING(1, &name[0]);
      UA_QualifiedName myName = UA_QUALIFIEDNAME(1, &mypair.first[0]);
      UA_Server_addVariableNode(_server, myNodeId, _parent,
          UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), myName,
          UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);
      UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                        "Trying to add node:  %s with name: %s" , name.c_str(), mypair.first.c_str());
    }

};

OPCUAServer::OPCUAServer(): _server(UA_Server_new()){
  std::random_device rd;
  std::uniform_int_distribution<uint> dist(20000, 22000);
  _port = dist(rd);
//  _port = 4848;
}

OPCUAServer::~OPCUAServer(){
  UA_Server_delete(_server);
}

void OPCUAServer::start(){
  UA_ServerConfig_setMinimal(UA_Server_getConfig(_server), _port, NULL);
  running = true;

  addVariables();

  UA_Server_run(_server, &running);
}

void OPCUAServer::addFolder(std::string name, UA_NodeId parent){
  std::string displayName = name.substr(name.find_last_of("/")+1,name.size());
  UA_ObjectAttributes attrObj = UA_ObjectAttributes_default;
  attrObj.description = UA_LOCALIZEDTEXT_ALLOC("en_US",&displayName[0]);
  attrObj.displayName = UA_LOCALIZEDTEXT_ALLOC("en_US",&displayName[0]);
  UA_QualifiedName qname = UA_QUALIFIEDNAME(1, &displayName[0]);
  UA_NodeId objNode = UA_NODEID_STRING(1, &name[0]);
  UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
  UA_Server_addObjectNode(_server, objNode, parent, UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char*)name.c_str()), UA_NODEID_NUMERIC(0, UA_NS0ID_FOLDERTYPE), attrObj, NULL, NULL);
}

void OPCUAServer::addVariables(){
  /* Add object node */
  // Create our toplevel instance
  UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
  oAttr.displayName = UA_LOCALIZEDTEXT_ALLOC("en_US", "Dummy");
  oAttr.description = UA_LOCALIZEDTEXT_ALLOC("en_US", "Dummy");
  UA_Server_addObjectNode(_server, UA_NODEID("ns=1;s=Dummy"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                          UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                          UA_QUALIFIEDNAME_ALLOC(1, "Dummy"),
                          UA_NODEID_NUMERIC(0, UA_NS0ID_FOLDERTYPE), oAttr, NULL, NULL);
  addFolder("Dummy/scalar", UA_NODEID("ns=1;s=Dummy"));
  boost::fusion::for_each(dummyMap, VariableAttacher(UA_NODEID("ns=1;s=Dummy/scalar"), _server, false, false));
  addFolder("Dummy/array", UA_NODEID("ns=1;s=Dummy"));
  boost::fusion::for_each(dummyMap, VariableAttacher(UA_NODEID("ns=1;s=Dummy/array"), _server, true, false));
  addFolder("Dummy/scalar_ro", UA_NODEID("ns=1;s=Dummy"));
  boost::fusion::for_each(dummyMap, VariableAttacher(UA_NODEID("ns=1;s=Dummy/scalar_ro"), _server, false, true));
  addFolder("Dummy/array_ro", UA_NODEID("ns=1;s=Dummy"));
  boost::fusion::for_each(dummyMap, VariableAttacher(UA_NODEID("ns=1;s=Dummy/array_ro"), _server, true, true));

}

UA_Variant* OPCUAServer::getValue(std::string nodeName){
  UA_Variant* data = UA_Variant_new();
  UA_Server_readValue(_server, UA_NODEID_STRING(1, &nodeName[0]), data);
  return data;
}

void ThreadedOPCUAServer::start(){
  if(_serverThread.joinable())
    _serverThread.join();
  _serverThread = std::thread{&OPCUAServer::start, &_server};
  if(!checkConnection(ServerState::On))
    throw std::runtime_error("Failed to connect to the test server!");
  UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                          "Test server is set up and running.");
}

ThreadedOPCUAServer::~ThreadedOPCUAServer(){
  _server.stop();
  _serverThread.join();
}

bool ThreadedOPCUAServer::checkConnection(const ServerState &state){
  UA_Client* client = UA_Client_new();
  UA_Client_getConfig(client);
  /** Connect **/
  UA_StatusCode retval;
  std::string serverAddress("opc.tcp://localhost:"+std::to_string(_server._port));
  uint time = 0;
  while(retval != UA_STATUSCODE_GOOD){
    retval = UA_Client_connect(client, serverAddress.c_str());
    if(state == ServerState::On){
      if(retval == UA_STATUSCODE_GOOD)
        break;
    } else {
      if(retval != UA_STATUSCODE_GOOD)
        break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    time++;
    if(time == 200){
      // break after 2s - server should be up now!
      return false;
    }
  }
  UA_Client_delete(client); /* Disconnects the client internally */
  return true;
}
