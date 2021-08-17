/*
 * OPC-UA-Backend.cc
 *
 *  Created on: Nov 19, 2018
 *      Author: zenker
 */

#include "OPC-UA-Backend.h"
#include "OPC-UA-BackendRegisterAccessor.h"
#include "SubscriptionManager.h"

#include <open62541/client_config_default.h>
#include <open62541/plugin/log_stdout.h>

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/RegisterInfo.h>
#include <ChimeraTK/DeviceAccessVersion.h>
#include <ChimeraTK/Exception.h>
#include <string>
#include <fstream>

#include <boost/make_shared.hpp>
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
  std::map<UA_Client*, OpcUABackend*> OpcUABackend::backendClients;

  void OpcUABackend::stateCallback(UA_Client *client, UA_SecureChannelState channelState,
              UA_SessionState sessionState, UA_StatusCode recoveryStatus) {
//    std::lock_guard<std::mutex> lock(OpcUABackend::backendClients[client]->_connection->client_lock);
    OpcUABackend::backendClients[client]->_connection->channelState = channelState;
    OpcUABackend::backendClients[client]->_connection->sessionState = sessionState;
    switch(channelState) {
      case UA_SECURECHANNELSTATE_CLOSED:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "The client is disconnected");
        OpcUABackend::backendClients[client]->_isFunctional = false;
        break;
      case UA_SECURECHANNELSTATE_HEL_SENT:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Waiting for HEL");
        OpcUABackend::backendClients[client]->_isFunctional = false;
        break;
      case UA_SECURECHANNELSTATE_OPN_SENT:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Waiting for OPN Response");
        OpcUABackend::backendClients[client]->_isFunctional = false;
        break;
      case UA_SECURECHANNELSTATE_OPEN:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "A SecureChannel to the server is open");
        OpcUABackend::backendClients[client]->_isFunctional = true;
        break;
      case UA_SECURECHANNELSTATE_FRESH:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "SecureChannel state: fresh");
        OpcUABackend::backendClients[client]->_isFunctional = true;
        break;
      case UA_SECURECHANNELSTATE_HEL_RECEIVED:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Hel received");
        OpcUABackend::backendClients[client]->_isFunctional = true;
        break;
      case UA_SECURECHANNELSTATE_ACK_SENT:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Waiting for ACK");
        OpcUABackend::backendClients[client]->_isFunctional = true;
        break;
      case UA_SECURECHANNELSTATE_ACK_RECEIVED:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "ACK received");
        OpcUABackend::backendClients[client]->_isFunctional = true;
        break;
      case UA_SECURECHANNELSTATE_CLOSING:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Closing secure channel");
        OpcUABackend::backendClients[client]->_isFunctional = true;
        break;
      default:
        break;
    }
    switch(sessionState) {
      case UA_SESSIONSTATE_ACTIVATED:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Session activated.");
        break;
      case UA_SESSIONSTATE_CLOSED:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Session disconnected.");
        break;
      case UA_SESSIONSTATE_CLOSING:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Session is closing...");
        break;
      case UA_SESSIONSTATE_CREATE_REQUESTED:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Session create requested.");
        break;
      case UA_SESSIONSTATE_ACTIVATE_REQUESTED:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Session activate requested.");
        break;
      case UA_SESSIONSTATE_CREATED:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Session created.");
        break;

      default:
        break;
    }

    if(!OpcUABackend::backendClients[client]->_isFunctional && OpcUABackend::backendClients[client]->_subscriptionManager){
      if (OpcUABackend::backendClients[client]->_subscriptionManager->isRunning())
        OpcUABackend::backendClients[client]->_subscriptionManager->deactivateAllAndPushException();
    }
  }

  OpcUABackend::OpcUABackend(const std::string &fileAddress, const unsigned long &port, const std::string &username, const std::string &password, const std::string &mapfile, const unsigned long &subscriptonPublishingInterval):
      _subscriptionManager(nullptr), _catalogue_filled(false), _mapfile(mapfile){
    _connection = std::make_unique<OPCUAConnection>(fileAddress, username, password, port, subscriptonPublishingInterval);
    _connection->config->stateCallback = stateCallback;

    OpcUABackend::backendClients[_connection->client.get()] = this;
    FILL_VIRTUAL_FUNCTION_TEMPLATE_VTABLE(getRegisterAccessor_impl);
    /* Registers are added before open() is called in ApplicationCore.
     * Since in the registration the catalog is needed we connect already
     * here and create the catalog.
     */
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
    UA_BrowseRequest_clear(&browseRequest);
    UA_BrowseResponse_clear(&brp);
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
                    "Wrong number of tokens (%s) in opcua mapfile %s line (-> line is ignored): \n %s",
                    std::to_string(nTokens).c_str(), _mapfile.c_str(), line.c_str());
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
          } catch (std::out_of_range &e){
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
      std::string localNodeName;
      if(node.identifierType == UA_NODEIDTYPE_STRING){
        localNodeName = std::string((char*)node.identifier.string.data, node.identifier.string.length);
      } else {
        localNodeName = std::string("numeric/") + std::to_string(node.identifier.numeric);
      }
      if(localNodeName.at(0) == '/'){
        localNodeName = localNodeName.substr(1,localNodeName.size()-1);
      }
      entry = boost::make_shared<OpcUABackendRegisterInfo>(_connection->serverAddress, localNodeName);
    } else {
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
      UA_Variant_delete(val);
      UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
          "Array length of variable: %s  is 0!. Variable is not added to the catalog."
          , entry->_nodeBrowseName.c_str());
      return;
    } else {
      entry->_arrayLength = val->arrayLength;
    }
    UA_Variant_delete(val);
    UA_NodeId_copy(&node,&entry->_id);

    if((entry->_dataType == 3 /*BYTE*/) ||
       (entry->_dataType == 5 /*UInt16*/) ||
       (entry->_dataType == 7 /*UInt32*/) ||
       (entry->_dataType == 9 /*UInt64*/)){
      entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                    true, false, 320, 300 );
    } else if ((entry->_dataType == 4 /*Int16*/) ||
        (entry->_dataType == 6 /*Int32*/) ||
        (entry->_dataType == 8 /*Int64*/) ||
        (entry->_dataType == 2 /*SByte*/)){
      entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                          true, true, 320, 300 );
    } else if (entry->_dataType == 12){
      entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::string,
                          true, true, 320, 300 );
    } else if ((entry->_dataType == 11 /*Double*/) || (entry->_dataType == 10 /*Float*/)){
      entry->dataDescriptor = RegisterInfo::DataDescriptor( ChimeraTK::RegisterInfo::FundamentalType::numeric,
                          false, true, 320, 300 );
    } else {
      UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "Failed to determine data type for node: %s  -> entry is not added to the catalogue." , entry->_nodeBrowseName.c_str());
      return;
    }
    entry->_accessModes.add(AccessMode::wait_for_new_data);
    //\ToDo: Test this here!!
    UA_Byte accessLevel;
    retval = UA_Client_readAccessLevelAttribute(_connection->client.get(),node,&accessLevel);
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

  void OpcUABackend::resetClient(){
    if(_subscriptionManager){
      _subscriptionManager->deactivate();
      _subscriptionManager->resetMonitoredItems();
    }
  }

  bool OpcUABackend::isConnected() const{
    return (_connection->sessionState == UA_SESSIONSTATE_ACTIVATED && _connection->channelState == UA_SECURECHANNELSTATE_OPEN);
  }

  void OpcUABackend::open() {
    /* Normally client is already connected in the constructor.
     * But open() is also called by ApplicationCore in case an error
     * was detected by the Backend (i.e. an exception was thrown
     * by one of the RegisterAccessors).
     */

    // if an error was seen by the accessor the error is in the queue but as long as no read happened setException is not called
    // but in the tests the device is reset without calling read in between -> so we need to check the connection here
    // -> to make sure the Subscription internal thread is stopped.
    if(!_isFunctional || !isConnected()){
      UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "Opening the device: %s" , _connection->serverAddress.c_str());
      if(_subscriptionManager){
        if(_subscriptionManager->_opcuaThread && _subscriptionManager->_opcuaThread->joinable()){
          _subscriptionManager->_opcuaThread->join();
          _subscriptionManager->_opcuaThread.reset(nullptr);
        }
      }
      connect();
    }

    if(!_catalogue_filled){
      fillCatalogue();
      _catalogue_filled = true;
    }
    _opened = true;
    // wait at maximum 100ms for the client to come up
    uint i = 0;
    while(!isConnected()){
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      ++i;
      if(i>4)
        throw ChimeraTK::runtime_error("Connection could not be established.");
    }
    _isFunctional = true;
  }

  void OpcUABackend::close() {
    //ToDo: What to do with the subscription manager?
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Closing the device: %s" , _connection->serverAddress.c_str());
    resetClient();
    if(_subscriptionManager)
      _subscriptionManager->removeMonitoredItems();
    //\ToDo: Check if we should reset the catalogue after closing. The UnifiedBackendTest will fail in that case.
//    _catalogue_mutable = RegisterCatalogue();
//    _catalogue_filled = false;
    _connection->close();
    _opened = false;
    _isFunctional = false;
  }

  void OpcUABackend::connect(){
    resetClient();
    UA_StatusCode retval;
    {
      std::lock_guard<std::mutex> lock(_connection->client_lock);
      /** Connect **/
      if(_connection->username.empty() || _connection->password.empty()){
        retval = UA_Client_connect(_connection->client.get(), _connection->serverAddress.c_str());
      } else {
        retval = UA_Client_connectUsername(_connection->client.get(), _connection->serverAddress.c_str(), _connection->username.c_str(), _connection->password.c_str());
      }
    }
    if(retval != UA_STATUSCODE_GOOD) {
      std::stringstream ss;
      ss << "Failed to connect to opc server: " <<  _connection->serverAddress << " with reason: " << UA_StatusCode_name(retval);
      throw ChimeraTK::runtime_error(ss.str());
    } else {
      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "Connection established:  %s " , _connection->serverAddress.c_str());
    }
    // if already setup subscriptions where used
    if(_subscriptionManager)
      _subscriptionManager->prepare();

  }

  bool OpcUABackend::isFunctional() const {
    // isFunctional is also set by setException!
    if (isConnected() && _isFunctional){
      return true;
    } else {
      return false;
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
      ss << "Requested number of words/elements ( " << numberOfWords << ") with offset " + wordOffsetInRegister << " exceeds the number of available words/elements: " << info->_arrayLength;
      throw ChimeraTK::logic_error(ss.str());
    }

    if(numberOfWords == 0)
      numberOfWords = info->_arrayLength;

    switch(info->_dataType){
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
}
