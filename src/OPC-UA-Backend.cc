/*
 * OPC-UA-Backend.cc
 *
 *  Created on: Nov 19, 2018
 *      Author: zenker
 */

#include "OPC-UA-Backend.h"
#include "OPC-UA-BackendRegisterAccessor.h"


#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/DeviceAccessVersion.h>

#include <string>

// You have to define an "extern C" function with this signature. It has to return
// CHIMERATK_DEVICEACCESS_VERSION for version checking when the library is loaded
// at run time. This function is used to determine that this is a valid DeviceAcces
// backend library. Just copy this code, sorry for the boiler plate.
extern "C"{
  const char * deviceAccessVersionUsedToCompile(){
    return CHIMERATK_DEVICEACCESS_VERSION;
  }
}

namespace ChimeraTK{
  OpcUABackend::BackendRegisterer OpcUABackend::backendRegisterer;

  OpcUABackend::OpcUABackend(const std::string &fileAddress, const unsigned long &port, const std::string &username, const std::string &password):
      _catalogue_filled(false), _serverAddress(fileAddress), _port(port), _username(username), _password(password), _client(nullptr), _config(UA_ClientConfig_standard){
//    _config.timeout = 10;
    FILL_VIRTUAL_FUNCTION_TEMPLATE_VTABLE(getRegisterAccessor_impl);
  }

  /**
   *  RegisterInfo-derived class to be put into the RegisterCatalogue
   */
  class OpcUABackendRegisterInfo : public RegisterInfo {
    //\ToDo: Adopt for OPC UA
    public:
      OpcUABackendRegisterInfo(const std::string &serverAddress, const std::string &node_id):
      _serverAddress(serverAddress), _node_id(node_id){
        path = RegisterPath(serverAddress)/RegisterPath(node_id);
      }
      virtual ~OpcUABackendRegisterInfo() {}

      RegisterPath getRegisterName() const override { return RegisterPath(_node_id); }

      std::string getRegisterPath() const { return path; }

      unsigned int getNumberOfElements() const override { return _arrayLength; }

      unsigned int getNumberOfChannels() const override { return 1; }

      unsigned int getNumberOfDimensions() const override { return _arrayLength > 1 ? 1 : 0; }

      const RegisterInfo::DataDescriptor& getDataDescriptor() const override { return dataDescriptor; }

      RegisterPath path;
      std::string _serverAddress;
      std::string _node_id;
      std::string _description;
      std::string _unit;
      std::string _dataType;
      RegisterInfo::DataDescriptor dataDescriptor;
      bool _isReadonly;
      size_t _arrayLength;

  };

  std::set<UA_UInt32> OpcUABackend::browse(UA_UInt32 node, UA_UInt16 ns) const{
    std::set<UA_UInt32> nodes;
    /* Browse some objects */
    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    bReq.requestedMaxReferencesPerNode = 0;
    bReq.nodesToBrowse = UA_BrowseDescription_new();
    bReq.nodesToBrowseSize = 1;
    bReq.nodesToBrowse[0].nodeId = UA_NODEID_NUMERIC(ns, node); /* browse objects folder */
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL; /* return everything */
    UA_BrowseResponse bResp = UA_Client_Service_browse(_client, bReq);
    for (size_t i = 0; i < bResp.resultsSize; ++i) {
      for (size_t j = 0; j < bResp.results[i].referencesSize; ++j) {
        UA_ReferenceDescription *ref = &(bResp.results[i].references[j]);
        if(ref->nodeId.nodeId.namespaceIndex == 1){
          if(ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_NUMERIC) {
            nodes.insert(ref->nodeId.nodeId.identifier.numeric);
          }
        }
      }
    }
    UA_BrowseRequest_deleteMembers(&bReq);
    UA_BrowseResponse_deleteMembers(&bResp);
    return nodes;
  }

  std::set<UA_UInt32> OpcUABackend::findServerNodes(UA_UInt32 node) const{
    std::set<UA_UInt32> serverNodes;
    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    bReq.requestedMaxReferencesPerNode = 0;
    bReq.nodesToBrowse = UA_BrowseDescription_new();
    bReq.nodesToBrowseSize = 1;
    bReq.nodesToBrowse[0].nodeId = UA_NODEID_NUMERIC(1, node); /* browse objects folder */
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL; /* return everything */
    UA_BrowseResponse bResp = UA_Client_Service_browse(_client, bReq);
    for (size_t i = 0; i < bResp.resultsSize; ++i) {
      for (size_t j = 0; j < bResp.results[i].referencesSize; ++j) {
        UA_ReferenceDescription *ref = &(bResp.results[i].references[j]);
        std::string test((char*)ref->browseName.name.data);
        if(test.find("/") != std::string::npos){
          serverNodes.insert(ref->nodeId.nodeId.identifier.numeric);
        }

      }
    }
    UA_BrowseRequest_deleteMembers(&bReq);
    UA_BrowseResponse_deleteMembers(&bResp);
    return serverNodes;
  }

  void OpcUABackend::fillCatalogue() {
    std::lock_guard<std::mutex> lock(opcua_mutex);
    std::set<UA_UInt32> nodes = browse(UA_NS0ID_OBJECTSFOLDER, 0);
//    std::set<UA_UInt32> nodes;// = browse(_client, 0,UA_NS0ID_OBJECTSFOLDER);
    if(nodes.size() != 1){
      throw ChimeraTK::runtime_error("Found more than one NS 1 folder...this is not expected!");
    }
    UA_UInt32 test = *nodes.begin();
    while(true){
      nodes = findServerNodes(test);
      if(nodes.size() != 0)
        break;
      nodes = browse(test);
      test = *nodes.begin();
    }

    for(auto it = nodes.begin(), ite = nodes.end(); it != ite; it++){
      addCatalogueEntry(*it);
    }
  }

  void OpcUABackend::addCatalogueEntry(const UA_UInt32 &node) {
    UA_QualifiedName *outBrowseName = UA_QualifiedName_new();
    UA_StatusCode retval = UA_Client_readBrowseNameAttribute(_client, UA_NODEID_NUMERIC(1, node), outBrowseName);
    if(retval != UA_STATUSCODE_GOOD){
      UA_QualifiedName_delete(outBrowseName);
      std::cerr << "Reconnect during adding catalogue entries. Error is " << std::hex << retval <<  std::endl;
      reconnect();
      return;
    }
    std::string nodeName ((char*)outBrowseName->name.data, outBrowseName->name.length);
    UA_QualifiedName_delete(outBrowseName);
    //\ToDo: Why this happens (seen with llrf_server)??
    if(nodeName.empty())
      return;

    boost::shared_ptr<OpcUABackendRegisterInfo> entry(new OpcUABackendRegisterInfo(_serverAddress, nodeName.substr(1,nodeName.size()-1)));
    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    bReq.requestedMaxReferencesPerNode = 0;
    bReq.nodesToBrowse = UA_BrowseDescription_new();
    bReq.nodesToBrowseSize = 1;
    bReq.nodesToBrowse[0].nodeId = UA_NODEID_NUMERIC(1, node); /* browse objects folder */
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL; /* return everything */
    UA_BrowseResponse bResp = UA_Client_Service_browse(_client, bReq);
    for (size_t i = 0; i < bResp.resultsSize; ++i) {
      for (size_t j = 0; j < bResp.results[i].referencesSize; ++j) {
        // Loop over Description, EngineeringUnit, Name, Type, Value
        UA_ReferenceDescription *ref = &(bResp.results[i].references[j]);
        if(ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_NUMERIC) {
          UA_Variant *val = UA_Variant_new();
          UA_String str_val;
          UA_StatusCode retval = UA_Client_readValueAttribute(_client, UA_NODEID_NUMERIC(1, ref->nodeId.nodeId.identifier.numeric), val);
          if(retval == UA_STATUSCODE_GOOD && UA_Variant_isScalar(val) &&
                  val->type == &UA_TYPES[UA_TYPES_STRING]) {
            std::string tmp((char*)ref->browseName.name.data);
            if(tmp.compare("Description") == 0){
              str_val = *(UA_String*)val->data;
              if(str_val.length != 0){
                entry->_description = std::string((char*)str_val.data);
              }
            } else if (tmp.compare("EngineeringUnit") == 0){
              str_val = *(UA_String*)val->data;
              if(str_val.length != 0){
                entry->_unit = std::string((char*)str_val.data);
              }
            } else if (tmp.compare("Type") == 0){
              str_val = *(UA_String*)val->data;
              if(str_val.length != 0){
                entry->_dataType = std::string((char*)str_val.data);
              }
            }
          } else if (retval != UA_STATUSCODE_GOOD && UA_Variant_isScalar(val) &&
              val->type == &UA_TYPES[UA_TYPES_STRING]){
            throw ChimeraTK::runtime_error(std::string("Failed to read node information for node: ") + nodeName);
          }

          UA_Variant_delete(val);

        } else if (ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_STRING){
          UA_Byte *outUserAccessLevel = UA_Byte_new();
          retval = UA_Client_readUserAccessLevelAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(nodeName.c_str())),outUserAccessLevel);
          if(retval == UA_STATUSCODE_GOOD && (*outUserAccessLevel == (UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE) )){
            entry->_isReadonly = false;
          } else if (retval == UA_STATUSCODE_GOOD && (*outUserAccessLevel == UA_ACCESSLEVELMASK_READ)){
            entry->_isReadonly = true;
          } else {
//            throw ChimeraTK::runtime_error(std::string("Failed to read access rights for node: ") + nodeName);
            std::cerr << "Failed to read access rights for node: " << nodeName << " -> set readonly." << std::endl;
            entry->_isReadonly = true;
          }
          UA_Byte_delete(outUserAccessLevel);
          UA_Variant *val = UA_Variant_new();
          retval = UA_Client_readValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(nodeName.c_str())), val);
          if(retval == UA_STATUSCODE_GOOD){
            if(UA_Variant_isScalar(val)) {
              entry->_arrayLength = val->arrayLength;
            } else {
              entry->_arrayLength = 1;
            }
            UA_Variant_delete(val);
          } else {
            UA_Variant_delete(val);
            throw ChimeraTK::runtime_error(std::string("Failed to determine arrray length for node: ") + nodeName);
          }

        }
      }
    }
    UA_BrowseRequest_deleteMembers(&bReq);
    UA_BrowseResponse_deleteMembers(&bResp);
    if((entry->_dataType.compare("uint32_t") == 0) || (entry->_dataType.compare("uint64_t") == 0)){
      entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                    true, false, 320, 300 );
    } else if ((entry->_dataType.compare("int32_t") == 0) || (entry->_dataType.compare("int64_t") == 0)){
      entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                          true, true, 320, 300 );
    } else if (entry->_dataType.compare("string") == 0){
      entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::string,
                          true, true, 320, 300 );
    } else if ((entry->_dataType.compare("double") == 0) || (entry->_dataType.compare("float") == 0)){
      entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                          false, true, 320, 300 );
    } else {
//      throw ChimeraTK::runtime_error(std::string("Unknown data type: ") + entry->_dataType);
      std::cerr << "Failed to determine data type for node: " << nodeName << " -> entry is not added to the catalogue." << std::endl;
      return;
    }
    _catalogue_mutable.addRegister(entry);

  }

  void OpcUABackend::deleteClient(){
    UA_Client_delete(_client); /* Disconnects the client internally */
    _client = nullptr;
  }
  void OpcUABackend::open() {
    _client = UA_Client_new(_config);
    UA_StatusCode retval;

    /** Connect **/
    if(UA_Client_getState(_client) != UA_CLIENTSTATE_READY){
      deleteClient();
      throw ChimeraTK::runtime_error("Failed to set up opc client.");
    }
    if(_username.empty() || _password.empty()){
      retval = UA_Client_connect(_client, _serverAddress.c_str());
    } else {
      retval = UA_Client_connect_username(_client, _serverAddress.c_str(), _username.c_str(), _password.c_str());
    }
    if(retval != UA_STATUSCODE_GOOD) {
      deleteClient();
      throw ChimeraTK::runtime_error(std::string("Failed to connect to opc server: ") + _serverAddress.c_str());
    }

    if(!_catalogue_filled){
      fillCatalogue();
      _catalogue_filled = true;
    }
    _opened = true;
  }

  void OpcUABackend::close() {
    deleteClient();
    _catalogue_mutable = RegisterCatalogue();
    _catalogue_filled = false;
    _opened = false;
  }

  void OpcUABackend::reconnect(){
//    std::lock_guard<std::mutex> lock(opcua_mutex);
    /** Test connection **/
    if(UA_Client_getState(_client) != UA_CLIENTSTATE_CONNECTED){
      _opened = false;
      deleteClient();
      _client = UA_Client_new(_config);
      UA_StatusCode retval;
      /** Connect **/
      if(UA_Client_getState(_client) != UA_CLIENTSTATE_READY){
        deleteClient();
        throw ChimeraTK::runtime_error("Failed to set up opc client.");
      }
      if(_username.empty() || _password.empty()){
        retval = UA_Client_connect(_client, _serverAddress.c_str());
      } else {
        retval = UA_Client_connect_username(_client, _serverAddress.c_str(), _username.c_str(), _password.c_str());
      }
      if(retval != UA_STATUSCODE_GOOD) {
        deleteClient();
        throw ChimeraTK::runtime_error(std::string("Failed to connect to opc server: ") + _serverAddress.c_str());
      }
      _opened = true;
    }
  }

  template<typename UserType>
  boost::shared_ptr< NDRegisterAccessor<UserType> > OpcUABackend::getRegisterAccessor_impl(
      const RegisterPath &registerPathName) {
    std::string path = _serverAddress+registerPathName;

    OpcUABackendRegisterInfo* info;
    for(auto it = _catalogue_mutable.begin(), ite = _catalogue_mutable.end(); it != ite; it++){
      if(it->getRegisterName() == registerPathName){
        info = dynamic_cast<OpcUABackendRegisterInfo*>(&(*it));
        break;
      }
    }
    NDRegisterAccessor<UserType> *p;
    p = new OpcUABackendRegisterAccessor<UserType>(path, _client, registerPathName, info->_isReadonly, this);
    return boost::shared_ptr< NDRegisterAccessor<UserType> > ( p );
  }

  OpcUABackend::BackendRegisterer::BackendRegisterer() {
    BackendFactory::getInstance().registerBackendType("opcua", &OpcUABackend::createInstance, {"port", "nodeID", "isReadOnly", "backend"});
    std::cout << "opcua::BackendRegisterer: registered backend type opcua" << std::endl;
  }

  const RegisterCatalogue& OpcUABackend::getRegisterCatalogue() const {
    return _catalogue_mutable;
 }

  boost::shared_ptr<DeviceBackend> OpcUABackend::createInstance(std::string address, std::map<std::string,std::string> parameters) {
    if(parameters["port"].empty()) {
      throw ChimeraTK::logic_error("Missing OPC-UA port.");
    }

    unsigned long port = std::stoul(parameters["port"]);
    std::string serverAddress = std::string("opc.tcp://") + address + ":" + std::to_string(port);
    return boost::shared_ptr<DeviceBackend> (new OpcUABackend(serverAddress, port, parameters["username"], parameters["password"]));
  }
}
