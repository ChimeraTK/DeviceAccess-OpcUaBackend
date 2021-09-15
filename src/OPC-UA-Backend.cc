/*
 * OPC-UA-Backend.cc
 *
 *  Created on: Nov 19, 2018
 *      Author: zenker
 */

#include "OPC-UA-Backend.h"
#include "OPC-UA-BackendRegisterAccessor.h"
#include "SubscriptionManager.h"

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

    std::vector<std::string> ChimeraTK_DeviceAccess_sdmParameterNames{"port", "username", "password","map"};

    std::string ChimeraTK_DeviceAccess_version{CHIMERATK_DEVICEACCESS_VERSION};

    std::string backend_name = "opcua";
}

namespace ChimeraTK{
  OpcUABackend::BackendRegisterer OpcUABackend::backendRegisterer;

  OpcUABackend::OpcUABackend(const std::string &fileAddress, const unsigned long &port, const std::string &username, const std::string &password, const std::string &mapfile, const unsigned long &subscriptonPublishingInterval):
      _subscriptionManager(nullptr), _catalogue_filled(false), _mapfile(mapfile){
    _connection = std::make_unique<OPCUAConnection>();
    _connection->serverAddress = fileAddress;
    _connection->port = port;
    _connection->username = username;
    _connection->password = password;
    _connection->config = UA_ClientConfig_standard;
    _connection->publishingInterval = subscriptonPublishingInterval;
    FILL_VIRTUAL_FUNCTION_TEMPLATE_VTABLE(getRegisterAccessor_impl);
    /* Registers are added before open() is called in ApplicationCore.
     * Since in the registration the catalog is needed we connect already
     * here and create the catalog.
     */
    //\ToDo: When using open62541 v1.1 set up callback function here to receive callback on state change.
//    _config->stateCallback = ...
    std::lock_guard<std::mutex> lock(_connection->connection_lock);
    connect();
    fillCatalogue();
    _catalogue_filled = true;
    _isFunctional = true;
  }

  void OpcUABackend::browseRecursive(UA_NodeId startingNode) {
    // connection is locked in fillCatalogue
    UA_BrowseDescription *bd = UA_BrowseDescription_new();
    UA_BrowseDescription_init(bd);
    bd->nodeId = startingNode;
    bd->browseDirection = UA_BROWSEDIRECTION_FORWARD;
    bd->resultMask = UA_BROWSERESULTMASK_ALL;
//    bd->nodeClassMask = UA_NODECLASS_VARIABLE; //search only variable nodes

    UA_BrowseRequest browseRequest;
    UA_BrowseRequest_init(&browseRequest);
    browseRequest.nodesToBrowse = bd;
    browseRequest.nodesToBrowseSize = 1;

    UA_BrowseResponse brp = UA_Client_Service_browse(_connection->client.get(), browseRequest);
    if(brp.resultsSize > 0){
        for(size_t i = 0; i < brp.results[0].referencesSize; i++){
            if(brp.results[0].references[i].nodeClass == UA_NODECLASS_OBJECT){
                browseRecursive(brp.results[0].references[i].nodeId.nodeId);
            }
            if(brp.results[0].references[i].nodeClass == UA_NODECLASS_VARIABLE){
              if(brp.results[0].references[i].nodeId.nodeId.identifierType == UA_NODEIDTYPE_STRING){
                addCatalogueEntry(brp.results[0].references[i].nodeId.nodeId);
              }
            }
        }
    }
    bd->nodeId = UA_NODEID_NULL;
    UA_BrowseRequest_deleteMembers(&browseRequest);
    UA_BrowseResponse_deleteMembers(&brp);
  }

  void OpcUABackend::getNodesFromMapfile(){
    boost::char_separator<char> sep{"\t ", "", boost::drop_empty_tokens};
    std::string line;
    std::ifstream mapfile (_mapfile);
    if (mapfile.is_open()) {
      while (std::getline(mapfile,line)) {
        if(line.empty())
          continue;
        tokenizer tok{line, sep};
        size_t nTokens = std::distance(tok.begin(), tok.end());
        if (!(nTokens == 2 || nTokens == 3)){
          UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Wrong number of tokens (%zu) in opcua mapfile %s line (-> line is ignored): \n %s",
                    nTokens, _mapfile.c_str(), line.c_str());
          continue;
        }
        auto it = tok.begin();
        std::shared_ptr<std::string> nodeName = nullptr;
        try{
          if(nTokens == 3){
            nodeName = std::make_shared<std::string>(*it);
            it++;
          }
          UA_UInt32 id = std::stoul(*it);
          it++;
          UA_UInt16 ns = std::stoul(*it);
          addCatalogueEntry(UA_NODEID_NUMERIC(ns, id), nodeName);
        } catch (std::invalid_argument &e){
          try{
            it = tok.begin();
            if(nTokens == 3){
              nodeName = std::make_shared<std::string>(*it);
              it++;
            }
            std::string id = (*it);
            it++;
            UA_UInt16 ns = std::stoul(*it);
            addCatalogueEntry(UA_NODEID_STRING(ns,(char*)id.c_str()), nodeName);
          } catch (std::invalid_argument &innerError){
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                      "Failed reading the following line from mapping file %s:\n %s", _mapfile.c_str(), line.c_str());
          } catch (std::out_of_range &rangeError){
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                      "Failed reading the following line from mapping file %s (Namespace id is out of range!):\n %s", _mapfile.c_str(), line.c_str());
          }
        } catch (std::out_of_range &e){
          UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Failed reading the following line from mapping file %s (Namespace id or Node id is out of range!):\n %s", _mapfile.c_str(), line.c_str());
        }
      }
      mapfile.close();
    } else {
      ChimeraTK::runtime_error(std::string("Failed reading opcua mapfile: ") + _mapfile);
    }
  }

  void OpcUABackend::fillCatalogue() {
    std::lock_guard<std::mutex> lock(_connection->client_lock);
    if(_mapfile.empty()){
      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Setting up OPC-UA catalog by browsing the server.");
      browseRecursive();
    } else {
      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Setting up OPC-UA catalog by reading the map file: %s", _mapfile.c_str());

      getNodesFromMapfile();
    }
  }

  void OpcUABackend::addCatalogueEntry(const UA_NodeId &node, std::shared_ptr<std::string> nodeName) {
    // connection is locked in fillCatalogue
    boost::shared_ptr<OpcUABackendRegisterInfo> entry;
    if(nodeName == nullptr){
      // used when reading nodes from map file
      std::string localNodeName;
      if(node.identifierType == UA_NODEIDTYPE_STRING){
        localNodeName = std::string((char*)node.identifier.string.data, node.identifier.string.length);
      } else {
        localNodeName = std::string("numeric/") + std::to_string(node.identifier.numeric);
      }
      if(localNodeName.at(0) == '/'){
        localNodeName = localNodeName.substr(1,localNodeName.size()-1);
      }
      // remove "Value" from node name
      auto match = localNodeName.find("Value");
      if(match == (localNodeName.length()-5)){
        // remove "Value" only if it is at the end of the node name
        localNodeName = localNodeName.substr(0, match);
      }
      entry = boost::make_shared<OpcUABackendRegisterInfo>(_connection->serverAddress, localNodeName);
    } else {
      // used when reading nodes from server
      entry = boost::make_shared<OpcUABackendRegisterInfo>(_connection->serverAddress, *(nodeName.get()));
    }

    UA_NodeId* id = UA_NodeId_new();
    UA_StatusCode retval = UA_Client_readDataTypeAttribute(_connection->client.get(),node,id);
    if(retval != UA_STATUSCODE_GOOD){
      UA_NodeId_delete(id);
      UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
          "Failed to read data type from variable: %s with reason: %s. Variable is not added to the catalog."
          , entry->_nodeBrowseName.c_str(), UA_StatusCode_name(retval));
      return;
    }
    entry->_dataType = id->identifier.numeric;
    UA_NodeId_delete(id);

    UA_LocalizedText* text = UA_LocalizedText_new();
    retval = UA_Client_readDescriptionAttribute(_connection->client.get(),node,text);
    if(retval != UA_STATUSCODE_GOOD){
      UA_LocalizedText_delete(text);
      UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
          "Failed to read data description from variable: %s with reason: %s. Variable is not added to the catalog."
          , entry->_nodeBrowseName.c_str(), UA_StatusCode_name(retval));
      return;
    }
    entry->_description =  std::string((char*)text->text.data, text->text.length);
    UA_LocalizedText_delete(text);

    UA_Variant *val = UA_Variant_new();
    retval = UA_Client_readValueAttribute(_connection->client.get(), node, val);
    if(retval != UA_STATUSCODE_GOOD){
      UA_Variant_delete(val);
      UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
          "Failed to read data from variable: %s with reason: %s. Variable is not added to the catalog."
          , entry->_nodeBrowseName.c_str(), UA_StatusCode_name(retval));
      return;
    }

    if(UA_Variant_isScalar(val)){
      entry->_arrayLength = 1;
    } else if(val->arrayLength == 0){
      UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
          "Array length of variable: %s  is 0!. Variable is not added to the catalog."
          , entry->_nodeBrowseName.c_str());
      return;
    } else {
      entry->_arrayLength = val->arrayLength;
    }
    UA_NodeId_copy(&node,&entry->_id);
    UA_Variant_delete(val);
    
    // Maximum number of decimal digits to display a float without loss in non-exponential display, including
    // sign, leading 0, decimal dot and one extra digit to avoid rounding issues (hence the +4).
    // This computation matches the one performed in the NumericAddressedBackend catalogue.
    size_t floatMaxDigits = std::max(std::log10(std::numeric_limits<float>::max()),
                                -std::log10(std::numeric_limits<float>::denorm_min())) + 4;

    switch(entry->_dataType){
      case 1: /*BOOL*/
        entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::boolean,
                            true, true, 320, 300 );
        break;
      case 2: /*SByte aka int8*/
        entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                            true, true, 4, 300 );
        break;
      case 3: /*BYTE aka uint8*/
        entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                            true, false, 3, 300 );
        break;
      case 4: /*Int16*/
        entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                            true, true, 5, 300 );
        break;
      case 5: /*UInt16*/
        entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                            true, false, 6, 300 );
        break;
      case 6: /*Int32*/
        entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                            true, true, 10, 300 );
        break;
      case 7: /*UInt32*/
        entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                            true, false, 11, 300 );
        break;
      case 8: /*Int64*/
        entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                            true, true, 320, 300 );
        break;
      case 9: /*UInt64*/
        entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                            true, false, 320, 300 );
        break;
      case 10: /*Float*/
        entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                            false, true, floatMaxDigits, 300 );
        break;
      case 11: /*Double*/
        entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                            false, true, 300, 300 );
        break;
      case 12: /*String*/
        entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::string,
                            true, true, 320, 300 );
        break;
      default:
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                          "Failed to determine data type for node: %s  -> entry is not added to the catalogue." , entry->_nodeBrowseName.c_str());
        return;
    }
    entry->_accessModes.add(AccessMode::wait_for_new_data);
    //\ToDo: Test this here!!
    UA_Byte accessLevel;
    retval = UA_Client_readAccessLevelAttribute(_connection->client.get(),entry->_id,&accessLevel);
    if(retval != UA_STATUSCODE_GOOD){
      UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Failed to read access level from variable: %s with reason: %s. Variable is not added to the catalog."
                , entry->_nodeBrowseName.c_str(), UA_StatusCode_name(retval));
      return;
    } else {
      if(accessLevel == (UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE))
        entry->_isReadonly = false;
      else
        entry->_isReadonly = true;
    }
    _catalogue_mutable.addRegister(entry);

  }

  void OpcUABackend::deleteClient(){
    if(_connection->client && _subscriptionManager)
      _subscriptionManager->deactivate();
    std::lock_guard<std::mutex> lock(_connection->client_lock);
    _connection->client.reset();/* Disconnects the client internally */
    _isFunctional = false;
  }
  void OpcUABackend::open() {
    std::lock_guard<std::mutex> connectionLock(_connection->connection_lock);
    /* Normally client is already connected in the constructor.
     * But open() is also called by ApplicationCore in case an error
     * was detected by the Backend (i.e. an exception was thrown
     * by one of the RegisterAccessors).
     */
    UA_ConnectionState state;
    {
      std::lock_guard<std::mutex> clientLock(_connection->client_lock);
      state = UA_Client_getConnectionState(_connection->client.get());
    }
    // if an error was seen by the accessor the error is in the queue but as long as no read happened setException is not called
    // but in the tests the the device is reset without calling read in between -> so we need to check the connection here
    // -> to make sure the Subscription intgernal thread is stopped.
    if(!_isFunctional || state != UA_CONNECTION_ESTABLISHED){
      UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "Opening the device: %s" , _connection->serverAddress.c_str());
      connect();
    }

    if(!_catalogue_filled){
      fillCatalogue();
      _catalogue_filled = true;
    }
    _opened = true;
    _isFunctional = true;
  }

  void OpcUABackend::close() {
    //ToDo: What to do with the subscription manager?
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Closing the device: %s" , _connection->serverAddress.c_str());
    deleteClient();
    //\ToDo: Check if we should reset the catalogue after closing. The UnifiedBackendTest will fail in that case.
//    _catalogue_mutable = RegisterCatalogue();
//    _catalogue_filled = false;
    _opened = false;
    _isFunctional = false;
  }

  void OpcUABackend::connect(){
//    if(_client == nullptr || getConnectionState() != UA_CLIENTSTATE_CONNECTED || !isFunctional()){
      if(_connection->client)
        deleteClient();
      UA_StatusCode retval;
      {
        std::lock_guard<std::mutex> lock(_connection->client_lock);
        _connection->client.reset(UA_Client_new(_connection->config));
        /** Connect **/
        if(UA_Client_getState(_connection->client.get()) != UA_CLIENTSTATE_READY){
          deleteClient();
          throw ChimeraTK::runtime_error("Failed to set up OPC-UA client");
        }
        if(_connection->username.empty() || _connection->password.empty()){
          retval = UA_Client_connect(_connection->client.get(), _connection->serverAddress.c_str());
        } else {
          retval = UA_Client_connect_username(_connection->client.get(), _connection->serverAddress.c_str(), _connection->username.c_str(), _connection->password.c_str());
        }
      }
      if(retval != UA_STATUSCODE_GOOD) {
        deleteClient();
        std::stringstream ss;
        ss << "Failed to connect to opc server: " <<  _connection->serverAddress << " with reason: " << UA_StatusCode_name(retval);
        throw ChimeraTK::runtime_error(ss.str());
      } else {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Connection established:  %s " , _connection->serverAddress.c_str());
      }
      // if already setup subscriptions where used
      if(_subscriptionManager)
        _subscriptionManager->resetClient();
//    }
  }

  bool OpcUABackend::isFunctional() const {
    //\ToDo: Check why connection is not accessable here and why UA_Client_getState(_client) is not working...
    // \ToDo: Use stateCallback with version 1.1
//    if(_client->connection->state != UA_CONNECTION_ESTABLISHED)
//      return false;
//    else
//      return true;
    if (!_connection->client || !_isFunctional){
      return false;
    } else {
      return true;
    }
  }

  void OpcUABackend::activateAsyncRead()noexcept{
    if(!_opened || !_isFunctional)
      return;
    if(!_subscriptionManager)
      _subscriptionManager = std::make_unique<OPCUASubscriptionManager>(_connection);
    _subscriptionManager->activate();

    //Check if thread is already running-> happens if a second logicalNameMappingBackend is calling activateAsyncRead after a first one called activateAsyncRead already
    if(!_subscriptionManager->_opcuaThread){
      _subscriptionManager->start();
      // sleep twice the publishing interval to make sure intital values are written
      std::this_thread::sleep_for(std::chrono::milliseconds(2*_connection->publishingInterval));
    }
  }

  void OpcUABackend::activateSubscriptionSupport(){
    if(!_subscriptionManager)
      _subscriptionManager = std::make_unique<OPCUASubscriptionManager>(_connection);
  }

  void OpcUABackend::setException(){
    _isFunctional = false;
    if(_subscriptionManager)
      _subscriptionManager->deactivateAllAndPushException();
    deleteClient();
  }

  template<typename UserType>
  boost::shared_ptr< NDRegisterAccessor<UserType> > OpcUABackend::getRegisterAccessor_impl(
      const RegisterPath &registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags) {
    std::string path = _connection->serverAddress+registerPathName;

    if(_catalogue_mutable.getNumberOfRegisters() == 0){
      throw ChimeraTK::logic_error("No registers found in the catalog.");
    }

    OpcUABackendRegisterInfo* info = nullptr;
    for(auto it = _catalogue_mutable.begin(), ite = _catalogue_mutable.end(); it != ite; it++){
      if(it->getRegisterName() == registerPathName){
        info = dynamic_cast<OpcUABackendRegisterInfo*>(&(*it));
        break;
      }
    }
    if(info == nullptr){
      throw ChimeraTK::logic_error(std::string("Requested register (") + registerPathName + ") was not found in the catalog.");
    }

    if(numberOfWords + wordOffsetInRegister > info->_arrayLength ||
       (numberOfWords == 0 && wordOffsetInRegister > 0)){
      std::stringstream ss;
      ss << "Requested number of words/elements ( " << numberOfWords << ") with offset " << wordOffsetInRegister << " exceeds the number of available words/elements: " << info->_arrayLength;
      throw ChimeraTK::logic_error(ss.str());
    }

    if(numberOfWords == 0)
      numberOfWords = info->_arrayLength;

    switch(info->_dataType){
      case 1:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_Boolean, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 2:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_SByte, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 3:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_Byte,  UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 4:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_Int16, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 5:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_UInt16, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 6:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_Int32, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 7:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_UInt32, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 8:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_Int64, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 9:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_UInt64, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 10:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_Float, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 11:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_Double, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 12:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_String, UserType>>(path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      default:
        throw ChimeraTK::runtime_error(std::string("Type ") + std::to_string(info->_dataType) + " not implemented.");
        break;
    }
  }

  OpcUABackend::BackendRegisterer::BackendRegisterer() {
    BackendFactory::getInstance().registerBackendType("opcua", &OpcUABackend::createInstance, {"port", "username", "password", "map", "publishingInterval"});
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "BackendRegisterer: registered backend type opcua");
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
    unsigned long publishingInterval = 500;
    if(!parameters["publishingInterval"].empty())
      publishingInterval = std::stoul(parameters["publishingInterval"]);
    return boost::shared_ptr<DeviceBackend> (new OpcUABackend(serverAddress, port, parameters["username"], parameters["password"], parameters["map"], publishingInterval));
  }

  OpcUABackend::~OpcUABackend(){
    deleteClient();
  }
}
