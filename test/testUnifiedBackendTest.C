/*
 * testUnifiedBackendTest.C
 *
 *  Created on: Dec 9, 2020
 *      Author: Klaus Zenker (HZDR)
 */

#include "open62541.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <vector>

#include <boost/fusion/container/map.hpp>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/include/at_key.hpp>

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

TypeMapWithName m(
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

UA_Boolean running = true;

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
//    auto mypair = fusion::at_key<UAType>(m);
    auto mypair = pair.second;
    /* Define the attribute of the myInteger variable node */
    UA_VariableAttributes attr;
    UA_VariableAttributes_init(&attr);
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
    attr.description = UA_LOCALIZEDTEXT("en_US",&mypair.first[0]);
    attr.displayName = UA_LOCALIZEDTEXT("en_US",&mypair.first[0]);
    attr.dataType = mypair.second.typeId;
    attr.userWriteMask = UA_ACCESSLEVELMASK_WRITE;
    attr.writeMask = UA_ACCESSLEVELMASK_WRITE;
    if(_readOnly){
      attr.accessLevel = 1;
      attr.userAccessLevel = 1;
    } else {
      attr.accessLevel = 3;
      attr.userAccessLevel = 3;
    }

    /* Add the variable node to the information model */
    UA_NodeId nodeId = UA_NODEID_STRING(1, &name[0]);
    UA_QualifiedName nodeName = UA_QUALIFIEDNAME(1, &mypair.first[0]);
    UA_Server_addVariableNode(_server, nodeId, _parent,
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), nodeName,
                              UA_NODEID_NULL, attr, NULL, NULL);
    std::cout << "Trying to add node: " << name << " with name: " << mypair.first << std::endl;
  }

  void operator()(fusion::pair<UA_String, std::pair<std::string,UA_DataType> >& pair) const{
//      auto mypair = fusion::at_key<UA_String>(m);
      auto mypair = pair.second;
      /* Define the attribute of the myInteger variable node */
      UA_VariableAttributes attr;
      UA_VariableAttributes_init(&attr);
      std::string strData = "42";
      std::vector<UA_String> v = {UA_STRING("42"), UA_STRING("42"),UA_STRING("42"),UA_STRING("42"),UA_STRING("42")};
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

      attr.description = UA_LOCALIZEDTEXT("en_US",&mypair.first[0]);
      attr.displayName = UA_LOCALIZEDTEXT("en_US",&mypair.first[0]);
      attr.dataType = mypair.second.typeId;
      attr.userWriteMask = UA_ACCESSLEVELMASK_WRITE;
      attr.writeMask = UA_ACCESSLEVELMASK_WRITE;
      if(_readOnly){
        attr.accessLevel = 1;
        attr.userAccessLevel = 1;
      } else {
        attr.accessLevel = 3;
        attr.userAccessLevel = 3;
      }

      /* Add the variable node to the information model */
      UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, &name[0]);
      UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, &mypair.first[0]);
      UA_Server_addVariableNode(_server, myIntegerNodeId, _parent,
          UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), myIntegerName,
                                UA_NODEID_NULL, attr, NULL, NULL);
      std::cout << "Trying to add node: " << name << " with name: " << mypair.first << std::endl;
    }

};

struct OPCUAServer{

  OPCUAServer(): _config(UA_ServerConfig_standard), _server(nullptr){
  }

  ~OPCUAServer(){
    UA_Server_delete(_server);
    _nl.deleteMembers(&_nl);
  }

  UA_ServerConfig _config;
  UA_Server *_server;
  UA_ServerNetworkLayer _nl;

  void _start(uint port){
    _nl = UA_ServerNetworkLayerTCP(UA_ConnectionConfig_standard, port);
    _config.networkLayers = &_nl;
    _config.networkLayersSize = 1;
    _server = UA_Server_new(_config);

    addVariables();

    UA_Server_run(_server, &running);
  }

  void start(){
//    _start(drawPort());
    _start(4848);
  }

  /*
   * Stop the server by setting running to false
   * This can be done by a signal handler or by setting running to false from outside -
   * so no interaction with the server is needed...
   */
  void stop(){
    running = false;
  }

  uint drawPort(){
    std::random_device rd;
    std::uniform_int_distribution<uint> dist(20000, 99999);
    return dist(rd);
  }

  void addFolder(std::string name, UA_NodeId parent){
    std::string displayName = name.substr(name.find_last_of("/")+1,name.size());
    UA_ObjectAttributes attrObj;
    UA_ObjectAttributes_init(&attrObj);
    attrObj.description = UA_LOCALIZEDTEXT("en_US",&displayName[0]);
    attrObj.displayName = UA_LOCALIZEDTEXT("en_US",&displayName[0]);
    UA_QualifiedName qname = UA_QUALIFIEDNAME(1, &displayName[0]);
    UA_NodeId objNode = UA_NODEID_STRING(1, &name[0]);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_Server_addObjectNode(_server, objNode, parent, UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        UA_QUALIFIEDNAME(1, (char*)name.c_str()), UA_NODEID_NUMERIC(0, UA_NS0ID_FOLDERTYPE), attrObj, NULL, NULL);
  }

  void addVariables(){
    /* Add object node */
    // Create our toplevel instance
    UA_ObjectAttributes oAttr;
    UA_ObjectAttributes_init(&oAttr);
    // Classcast to prevent Warnings
    oAttr.displayName = UA_LOCALIZEDTEXT((char*)"en_US", "Dummy");
    oAttr.description = UA_LOCALIZEDTEXT((char*)"en_US", "Dummy");
    UA_Server_addObjectNode(_server, UA_NODEID_STRING(1, "Dummy"),
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                            UA_QUALIFIEDNAME(1, "Dummy"),
                            UA_NODEID_NULL, oAttr, NULL, NULL);
//
    addFolder("Dummy/scalar", UA_NODEID_STRING(1,"Dummy"));
    boost::fusion::for_each(m, VariableAttacher(UA_NODEID_STRING(1,"Dummy/scalar"), _server, false, true));
//    addVariable("Dummy/scalar/int32", UA_NODEID_STRING(1,"Dummy/scalar"));
    addFolder("Dummy/array", UA_NODEID_STRING(1,"Dummy"));
    boost::fusion::for_each(m, VariableAttacher(UA_NODEID_STRING(1,"Dummy/array"), _server, true, true));
//    addVariable("Dummy/array/int32", UA_NODEID_STRING(1,"Dummy/array"));
  }
};

class ThreadedOPCUAServer {
private:
  OPCUAServer _server;
  std::thread _serverThread;

public:
  ThreadedOPCUAServer(): _serverThread(&OPCUAServer::start, &_server){

  }

  ~ThreadedOPCUAServer(){
    _server.stop();
    _serverThread.join();
  }
};

int main(){
  ThreadedOPCUAServer srv;
  std::cout << "Server was started..." << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(180));
  std::cout << "Stopping the server." << std::endl;
  return 0;
}
