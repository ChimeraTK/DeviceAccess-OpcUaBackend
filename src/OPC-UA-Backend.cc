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
#include <fstream>

#include <boost/tokenizer.hpp>
typedef boost::tokenizer<boost::char_separator<char>> tokenizer;

extern "C"{
    boost::shared_ptr<ChimeraTK::DeviceBackend> ChimeraTK_DeviceAccess_createBackend(
        std::string address, std::map<std::string, std::string> parameters) {
      return ChimeraTK::OpcUABackend::createInstance(address, parameters);
    }

    std::vector<std::string> ChimeraTK_DeviceAccess_sdmParameterNames{"port", "username", "password","mapfile"};

    std::string ChimeraTK_DeviceAccess_version{CHIMERATK_DEVICEACCESS_VERSION};

    std::string backend_name = "opcua";
}

namespace ChimeraTK{
  OpcUABackend::BackendRegisterer OpcUABackend::backendRegisterer;

  OpcUABackend::OpcUABackend(const std::string &fileAddress, const unsigned long &port, const std::string &username, const std::string &password, const std::string &mapfile):
      _catalogue_filled(false), _serverAddress(fileAddress), _port(port), _username(username), _password(password), _mapfile(mapfile), _client(nullptr), _config(UA_ClientConfig_standard){
//    _config.timeout = 10;
    FILL_VIRTUAL_FUNCTION_TEMPLATE_VTABLE(getRegisterAccessor_impl);
  }

  UASet OpcUABackend::browse(const UA_NodeId &node) const{
    UASet nodes;
    /* Browse some objects */
    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    bReq.requestedMaxReferencesPerNode = 0;
    bReq.nodesToBrowse = UA_BrowseDescription_new();
    bReq.nodesToBrowseSize = 1;
    bReq.nodesToBrowse[0].nodeId = node; /* browse objects folder */
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL; /* return everything */
    UA_BrowseResponse bResp = UA_Client_Service_browse(_client, bReq);
    for (size_t i = 0; i < bResp.resultsSize; ++i) {
      for (size_t j = 0; j < bResp.results[i].referencesSize; ++j) {
        UA_ReferenceDescription *ref = &(bResp.results[i].references[j]);
        if(ref->nodeId.nodeId.namespaceIndex == 1){
          if(ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_NUMERIC) {
            if(ref->nodeId.nodeId.identifier.numeric == 125){
              nodes.insert(ref->nodeId.nodeId);
            }
          }
        }
      }
    }
    UA_BrowseRequest_deleteMembers(&bReq);
    UA_BrowseResponse_deleteMembers(&bResp);
    return nodes;
  }

  UASet OpcUABackend::findServerNodes(UA_NodeId node) const{
    UASet serverNodes;
    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    bReq.requestedMaxReferencesPerNode = 0;
    bReq.nodesToBrowse = UA_BrowseDescription_new();
    bReq.nodesToBrowseSize = 1;
    bReq.nodesToBrowse[0].nodeId = node; /* browse objects folder */
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL; /* return everything */
    UA_BrowseResponse bResp = UA_Client_Service_browse(_client, bReq);
    for (size_t i = 0; i < bResp.resultsSize; ++i) {
      for (size_t j = 0; j < bResp.results[i].referencesSize; ++j) {
        UA_ReferenceDescription *ref = &(bResp.results[i].references[j]);
        std::string test((char*)ref->browseName.name.data);
        if(test.find("/") != std::string::npos){
          serverNodes.insert(ref->nodeId.nodeId);
        }

      }
    }
    UA_BrowseRequest_deleteMembers(&bReq);
    UA_BrowseResponse_deleteMembers(&bResp);
    return serverNodes;
  }

  void OpcUABackend::getNodesFromMapfile(){
    UASet nodes;
    std::vector<std::pair<std::string,std::string> > nodeList;

    boost::char_separator<char> sep{"\t ", "", boost::drop_empty_tokens};
    std::string line;
    std::ifstream mapfile (_mapfile);
    if (mapfile.is_open()) {
      while (std::getline(mapfile,line)) {
        tokenizer tok{line, sep};
        size_t nTokens = std::distance(tok.begin(), tok.end());
        if (nTokens != 2){
          ChimeraTK::runtime_error(std::string("Wrong number (" + std::to_string(nTokens)+ ") of tokens in opcua mapfile line: ") + line);
        }
        size_t token = 0;
        std::pair<std::string, std::string> p;
        for (const auto &t : tok){
          if(token == 0){
            std::cout << "Adding device variable: " << t << std::endl;
            p.first = t;
          } else {
            std::cout << "\t data type: " << t << std::endl;
            p.second = t;
          }
          token++;
        }
        nodeList.push_back(p);
      }
      mapfile.close();
    } else {
      ChimeraTK::runtime_error(std::string("Failed reading opcua mapfile: ") + _mapfile);
    }

    for(auto it = nodeList.begin(); it != nodeList.end(); it++){
      UA_NodeId node = UA_NODEID_STRING(4,(char*)it->first.c_str());
      UA_QualifiedName *outBrowseName = UA_QualifiedName_new();
      UA_StatusCode retval = UA_Client_readBrowseNameAttribute(_client, node, outBrowseName);
      if(retval != UA_STATUSCODE_GOOD){
        UA_QualifiedName_delete(outBrowseName);
        std::cerr << "Failed reading node: " << it->first.c_str() << ". It will not be added to the catalog." <<  std::endl;
        continue;
      }
      UA_QualifiedName_delete(outBrowseName);
      boost::shared_ptr<OpcUABackendRegisterInfo> entry = boost::make_shared<OpcUABackendRegisterInfo>(_serverAddress, it->first);
      entry->_dataType = it->second;
      if(it->second.compare("float") == 0){
        entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                          false, true, 320, 300 );
      } else {
        entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                          true, true, 320, 300 );
      }
      entry->_isReadonly = true;
      entry->_arrayLength = 1;
      UA_NodeId_copy(&(node),&entry->_id);
      _catalogue_mutable.addRegister(entry);
    }
  }

  void OpcUABackend::fillCatalogue() {
    std::lock_guard<std::mutex> lock(opcua_mutex);
    UASet nodes;
    if(_mapfile.empty()){
      std::cout << "Setting up OPC-UA catalog by browsing the server..." << std::endl;
      UA_NodeId variables = UA_NODEID_NUMERIC(1,125);
      nodes = findServerNodes(variables);
      for(auto it = nodes.begin(), ite = nodes.end(); it != ite; it++){
        addCatalogueEntry(*it);
      }
    } else {
      std::cout << "Setting up OPC-UA catalog by reading the map file: " << _mapfile.c_str() << std::endl;
      getNodesFromMapfile();
    }
  }

  void OpcUABackend::addCatalogueEntry(const UA_NodeId &node) {
    UA_QualifiedName *outBrowseName = UA_QualifiedName_new();
    UA_StatusCode retval = UA_Client_readBrowseNameAttribute(_client, node, outBrowseName);
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

    boost::shared_ptr<OpcUABackendRegisterInfo> entry = boost::make_shared<OpcUABackendRegisterInfo>(_serverAddress, nodeName.substr(1,nodeName.size()-1));
    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    bReq.requestedMaxReferencesPerNode = 0;
    bReq.nodesToBrowse = UA_BrowseDescription_new();
    bReq.nodesToBrowseSize = 1;
    bReq.nodesToBrowse[0].nodeId = node; /* browse objects folder */
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL; /* return everything */
    UA_BrowseResponse bResp = UA_Client_Service_browse(_client, bReq);
    for (size_t i = 0; i < bResp.resultsSize; ++i) {
      for (size_t j = 0; j < bResp.results[i].referencesSize; ++j) {
        // Loop over Description, EngineeringUnit, Name, Type, Value
        UA_ReferenceDescription *ref = &(bResp.results[i].references[j]);
        if(ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_NUMERIC) {
          UA_Variant *val = UA_Variant_new();
          UA_String str_val;
          UA_StatusCode retval = UA_Client_readValueAttribute(_client, ref->nodeId.nodeId, val);
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
          // this is the value node!
          UA_Byte *outUserAccessLevel = UA_Byte_new();
          retval = UA_Client_readUserAccessLevelAttribute(_client, ref->nodeId.nodeId,outUserAccessLevel);
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
          retval = UA_Client_readValueAttribute(_client, ref->nodeId.nodeId, val);
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
          UA_NodeId_copy(&ref->nodeId.nodeId,&entry->_id);
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
    if(info->_dataType.compare("int32_t") == 0){
      auto p = boost::make_shared<OpcUABackendRegisterAccessor<UA_Int32, UserType>>(path, _client, registerPathName, info);
      return p;
    } else if (info->_dataType.compare("uint32_t") == 0){
      auto p = boost::make_shared<OpcUABackendRegisterAccessor<UA_UInt32, UserType>>(path, _client, registerPathName, info);
      return p;
    } else if(info->_dataType.compare("int16_t") == 0){
      auto p = boost::make_shared<OpcUABackendRegisterAccessor<UA_Int16, UserType>>(path, _client, registerPathName, info);
      return p;
    } else if (info->_dataType.compare("uint16_t") == 0){
      auto p = boost::make_shared<OpcUABackendRegisterAccessor<UA_UInt16, UserType>>(path, _client, registerPathName, info);
      return p;
    } else if (info->_dataType.compare("double") == 0){
      auto p = boost::make_shared<OpcUABackendRegisterAccessor<UA_Double, UserType>>(path, _client, registerPathName, info);
      return p;
    } else if (info->_dataType.compare("float") == 0){
      auto p = boost::make_shared<OpcUABackendRegisterAccessor<UA_Float, UserType>>(path, _client, registerPathName, info);
      return p;
    } else if (info->_dataType.compare("string") == 0){
      auto p = boost::make_shared<OpcUABackendRegisterAccessor<UA_String, UserType>>(path, _client, registerPathName, info);
      return p;
    }
    throw ChimeraTK::runtime_error(std::string("Type") + info->_dataType + " not implemented.");

  }

  OpcUABackend::BackendRegisterer::BackendRegisterer() {
    BackendFactory::getInstance().registerBackendType("opcua", &OpcUABackend::createInstance, {"port", "username", "password", "mapfile"});
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
    return boost::shared_ptr<DeviceBackend> (new OpcUABackend(serverAddress, port, parameters["username"], parameters["password"], parameters["mapfile"]));
  }
}
