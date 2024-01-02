// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * OPC-UA-Backend.cc
 *
 *  Created on: Nov 19, 2018
 *      Author: Klaus Zenker (HZDR)
 */

#include "OPC-UA-Backend.h"

#include "OPC-UA-BackendRegisterAccessor.h"
#include "SubscriptionManager.h"
#include <open62541/client_config_default.h>
#include <open62541/plugin/log_stdout.h>

#include <ChimeraTK/BackendFactory.h>
#include <ChimeraTK/DeviceAccessVersion.h>
#include <ChimeraTK/Exception.h>
#include <ChimeraTK/RegisterInfo.h>

#include <boost/make_shared.hpp>
#include <boost/tokenizer.hpp>

#include <fstream>
#include <string>
typedef boost::tokenizer<boost::char_separator<char>> tokenizer;

extern "C" {
boost::shared_ptr<ChimeraTK::DeviceBackend> ChimeraTK_DeviceAccess_createBackend(
    std::string address, std::map<std::string, std::string> parameters) {
  return ChimeraTK::OpcUABackend::createInstance(address, parameters);
}

std::vector<std::string> ChimeraTK_DeviceAccess_sdmParameterNames{"port", "username", "password", "map"};

std::string ChimeraTK_DeviceAccess_version{CHIMERATK_DEVICEACCESS_VERSION};

std::string backend_name = "opcua";
}

namespace ChimeraTK {
  OpcUABackend::BackendRegisterer OpcUABackend::backendRegisterer;
  std::map<UA_Client*, OpcUABackend*> OpcUABackend::backendClients;

  void OpcUABackend::stateCallback(UA_Client* client, UA_SecureChannelState channelState, UA_SessionState sessionState,
      UA_StatusCode recoveryStatus) {
    if(OpcUABackend::backendClients.count(client) == 0) {
      UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "No client found in the stateCallback.");
      return;
    }
    OpcUABackend::backendClients[client]->_connection->channelState = channelState;
    OpcUABackend::backendClients[client]->_connection->sessionState = sessionState;
    switch(channelState) {
      case UA_SECURECHANNELSTATE_CLOSED:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "The client is disconnected");
        break;
      case UA_SECURECHANNELSTATE_HEL_SENT:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Waiting for HEL");
        break;
      case UA_SECURECHANNELSTATE_OPN_SENT:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Waiting for OPN Response");
        break;
      case UA_SECURECHANNELSTATE_OPEN:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "A SecureChannel to the server is open");
        break;
      case UA_SECURECHANNELSTATE_FRESH:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "SecureChannel state: fresh");
        break;
      case UA_SECURECHANNELSTATE_HEL_RECEIVED:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Hel received");
        break;
      case UA_SECURECHANNELSTATE_ACK_SENT:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Waiting for ACK");
        break;
      case UA_SECURECHANNELSTATE_ACK_RECEIVED:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "ACK received");
        break;
      case UA_SECURECHANNELSTATE_CLOSING:
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Closing secure channel");
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
    if(sessionState == UA_SESSIONSTATE_ACTIVATED && channelState == UA_SECURECHANNELSTATE_OPEN)
      OpcUABackend::backendClients[client]->_isFunctional = true;
    else
      OpcUABackend::backendClients[client]->_isFunctional = false;

    // when closing the device this does not need to be done
    if(OpcUABackend::backendClients[client]->_opened) {
      if(!OpcUABackend::backendClients[client]->_isFunctional &&
          OpcUABackend::backendClients[client]->_subscriptionManager) {
        if(OpcUABackend::backendClients[client]->_subscriptionManager->isRunning())
          OpcUABackend::backendClients[client]->_subscriptionManager->deactivateAllAndPushException(
              "Client session is not open any more.");
      }
    }
  }

  void OpcUABackend::inactivityCallback(UA_Client* client, UA_UInt32 subId, void* subContext) {
    // when closing the device this does not need to be done
    if(OpcUABackend::backendClients[client]->_opened) {
      if(OpcUABackend::backendClients[client]->_isFunctional &&
          OpcUABackend::backendClients[client]->_subscriptionManager) {
        if(OpcUABackend::backendClients[client]->_subscriptionManager->isRunning() &&
            OpcUABackend::backendClients[client]->_subscriptionManager->getSubscriptionID() == subId) {
          std::stringstream ss;
          ss << "No activity for subscriptions: " << subId;
          OpcUABackend::backendClients[client]->_subscriptionManager->deactivateAllAndPushException(ss.str());
          OpcUABackend::backendClients[client]->_isFunctional = false;
          /*
           *  Manually set session state to closed.
           *  When backend is recovered a new session will be created and thus the sessionState will be
           *  updated accordinly.
           */
          OpcUABackend::backendClients[client]->_connection->sessionState = UA_SessionState::UA_SESSIONSTATE_CLOSED;
        }
      }
    }
  }

  OpcUABackend::OpcUABackend(const std::string& fileAddress, const std::string& username, const std::string& password,
      const std::string& mapfile, const unsigned long& subscriptonPublishingInterval, const std::string& rootName,
      const ulong& rootNS, const long int& connectionTimeout)
  : _subscriptionManager(nullptr), _catalogue_filled(false), _mapfile(mapfile), _rootNode(rootName), _rootNS(rootNS) {
    _connection = std::make_unique<OPCUAConnection>(fileAddress, username, password, subscriptonPublishingInterval);
    _connection->config->stateCallback = stateCallback;
    _connection->config->subscriptionInactivityCallback = inactivityCallback;
    _connection->config->timeout = connectionTimeout;

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

  OpcUABackend::~OpcUABackend() {
    if(_opened) close();
    OpcUABackend::backendClients.erase(_connection->client.get());
  };
  void OpcUABackend::browseRecursive(UA_NodeId startingNode) {
    // connection is locked in fillCatalogue
    UA_BrowseDescription* bd = UA_BrowseDescription_new();
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
    if(brp.resultsSize > 0) {
      for(size_t i = 0; i < brp.results[0].referencesSize; i++) {
        if(brp.results[0].references[i].nodeClass == UA_NODECLASS_OBJECT) {
          browseRecursive(brp.results[0].references[i].nodeId.nodeId);
        }
        if(brp.results[0].references[i].nodeClass == UA_NODECLASS_VARIABLE) {
          if(brp.results[0].references[i].nodeId.nodeId.identifierType == UA_NODEIDTYPE_STRING) {
            addCatalogueEntry(brp.results[0].references[i].nodeId.nodeId);
          }
        }
      }
    }
    bd->nodeId = UA_NODEID_NULL;
    UA_BrowseRequest_clear(&browseRequest);
    UA_BrowseResponse_clear(&brp);
  }

  void OpcUABackend::getNodesFromMapfile() {
    boost::char_separator<char> sep{"\t ", "", boost::drop_empty_tokens};
    std::string line;
    std::ifstream mapfile(_mapfile);
    if(mapfile.is_open()) {
      while(std::getline(mapfile, line)) {
        if(line.empty() || line[0] == '#') continue;
        tokenizer tok{line, sep};
        size_t nTokens = std::distance(tok.begin(), tok.end());
        if(!(nTokens == 2 || nTokens == 3)) {
          UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
              "Wrong number of tokens (%s) in opcua mapfile %s line (-> line is ignored): \n %s",
              std::to_string(nTokens).c_str(), _mapfile.c_str(), line.c_str());
          continue;
        }
        auto it = tok.begin();
        std::shared_ptr<std::string> nodeName = nullptr;
        try {
          if(nTokens == 3) {
            nodeName = std::make_shared<std::string>(*it);
            it++;
          }
          UA_UInt32 id = std::stoul(*it);
          it++;
          UA_UInt16 ns = std::stoul(*it);
          addCatalogueEntry(UA_NODEID_NUMERIC(ns, id), nodeName);
        }
        catch(std::invalid_argument& e) {
          try {
            it = tok.begin();
            if(nTokens == 3) {
              nodeName = std::make_shared<std::string>(*it);
              it++;
            }
            std::string id = _rootNode + (*it);
            it++;
            UA_UInt16 ns = std::stoul(*it);
            addCatalogueEntry(UA_NODEID_STRING(ns, (char*)id.c_str()), nodeName);
          }
          catch(std::invalid_argument& innerError) {
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Failed reading the following line from mapping file %s:\n %s", _mapfile.c_str(), line.c_str());
          }
          catch(std::out_of_range& e) {
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Failed reading the following line from mapping file %s (Namespace id is out of range!):\n %s",
                _mapfile.c_str(), line.c_str());
          }
        }
        catch(std::out_of_range& e) {
          UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
              "Failed reading the following line from mapping file %s (Namespace id or Node id is out of range!):\n %s",
              _mapfile.c_str(), line.c_str());
        }
      }
      mapfile.close();
    }
    else {
      UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Failed reading opcua mapfile: %s", _mapfile.c_str());
      throw ChimeraTK::runtime_error(std::string("Failed reading opcua mapfile: ") + _mapfile);
    }
  }

  void OpcUABackend::fillCatalogue() {
    std::lock_guard<std::mutex> lock(_connection->client_lock);
    if(_mapfile.empty()) {
      if(_rootNode.empty()) {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Setting up OPC-UA catalog by browsing the server.");
        browseRecursive();
      }
      else {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
            "Setting up OPC-UA catalog by browsing the server using the root node: %s.", _rootNode.c_str());
        try {
          browseRecursive(UA_NODEID_STRING(_rootNS, (char*)_rootNode.c_str()));
        }
        catch(...) {
          throw ChimeraTK::runtime_error("root node not formated correct. Expected ns:nodeid or ns:nodename!");
        }
      }
    }
    else {
      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Setting up OPC-UA catalog by reading the map file: %s",
          _mapfile.c_str());

      getNodesFromMapfile();
    }
  }

  void OpcUABackend::addCatalogueEntry(const UA_NodeId& node, std::shared_ptr<std::string> nodeName) {
    // connection is locked in fillCatalogue
    std::string localNodeName;
    if(nodeName == nullptr) {
      // used when reading nodes from server
      if(node.identifierType == UA_NODEIDTYPE_STRING) {
        localNodeName = std::string((char*)node.identifier.string.data, node.identifier.string.length);
      }
      else {
        localNodeName = std::string("numeric/") + std::to_string(node.identifier.numeric);
      }
      if(localNodeName.at(0) == '/') {
        localNodeName = localNodeName.substr(1, localNodeName.size() - 1);
      }

      // remove "Dir" from node name
      auto localRootName = _rootNode;
      auto path = localRootName.rfind("Dir");
      if(path == (localRootName.length() - 3)) {
        // remove "Dir" only if it is at the end of the rootNode name
        localRootName = localRootName.substr(0, path);
      }
      localNodeName.erase(0, localRootName.length());
    }
    else {
      localNodeName = *(nodeName.get());
    }

    OpcUABackendRegisterInfo entry{_connection->serverAddress, localNodeName};
    UA_NodeId* id = UA_NodeId_new();
    UA_StatusCode retval = UA_Client_readDataTypeAttribute(_connection->client.get(), node, id);
    if(retval != UA_STATUSCODE_GOOD) {
      UA_NodeId_delete(id);
      UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
          "Failed to read data type from variable: %s with reason: %s. Variable is not added to the catalog.",
          entry._nodeBrowseName.c_str(), UA_StatusCode_name(retval));
      return;
    }
    entry._dataType = id->identifier.numeric;
    UA_NodeId_delete(id);

    UA_LocalizedText* text = UA_LocalizedText_new();
    retval = UA_Client_readDescriptionAttribute(_connection->client.get(), node, text);
    if(retval != UA_STATUSCODE_GOOD) {
      UA_LocalizedText_delete(text);
      UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
          "Failed to read data description from variable: %s with reason: %s.", entry._nodeBrowseName.c_str(),
          UA_StatusCode_name(retval));
    }
    else {
      entry._description = std::string((char*)text->text.data, text->text.length);
      UA_LocalizedText_delete(text);
    }

    UA_Variant* val = UA_Variant_new();
    retval = UA_Client_readValueAttribute(_connection->client.get(), node, val);
    if(retval != UA_STATUSCODE_GOOD) {
      UA_Variant_delete(val);
      UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
          "Failed to read data from variable: %s with reason: %s. Variable is not added to the catalog.",
          entry._nodeBrowseName.c_str(), UA_StatusCode_name(retval));
      return;
    }

    if(UA_Variant_isScalar(val)) {
      entry._arrayLength = 1;
    }
    else if(val->arrayLength == 0) {
      UA_Variant_delete(val);
      UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
          "Array length of variable: %s  is 0!. Variable is not added to the catalog.", entry._nodeBrowseName.c_str());
      return;
    }
    else {
      entry._arrayLength = val->arrayLength;
    }
    UA_Variant_delete(val);
    UA_NodeId_copy(&node, &entry._id);
    // Maximum number of decimal digits to display a float without loss in non-exponential display, including
    // sign, leading 0, decimal dot and one extra digit to avoid rounding issues (hence the +4).
    // This computation matches the one performed in the NumericAddressedBackend catalogue.
    size_t floatMaxDigits =
        std::max(std::log10(std::numeric_limits<float>::max()), -std::log10(std::numeric_limits<float>::denorm_min())) +
        4;

    switch(entry._dataType) {
      case 1: /*BOOL*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::boolean, true, true, 320, 300);
        break;
      case 2: /*SByte aka int8*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, true, 4, 300);
        break;
      case 3: /*BYTE aka uint8*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, false, 3, 300);
        break;
      case 4: /*Int16*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, true, 5, 300);
        break;
      case 5: /*UInt16*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, false, 6, 300);
        break;
      case 6: /*Int32*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, true, 10, 300);
        break;
      case 7: /*UInt32*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, false, 11, 300);
        break;
      case 8: /*Int64*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, true, 320, 300);
        break;
      case 9: /*UInt64*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, false, 320, 300);
        break;
      case 10: /*Float*/
        entry.dataDescriptor =
            DataDescriptor(DataDescriptor::FundamentalType::numeric, false, true, floatMaxDigits, 300);
        break;
      case 11: /*Double*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, false, true, 300, 300);
        break;
      case 12: /*String*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::string, true, true, 320, 300);
        break;
      default:
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
            "Failed to determine data type for node: %s  -> entry is not added to the catalogue.",
            entry._nodeBrowseName.c_str());
        return;
    }

    entry._accessModes.add(AccessMode::wait_for_new_data);
    //\ToDo: Test this here!!
    UA_Byte accessLevel;
    retval = UA_Client_readAccessLevelAttribute(_connection->client.get(), node, &accessLevel);
    if(retval != UA_STATUSCODE_GOOD) {
      UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
          "Failed to read access level from variable: %s with reason: %s. Variable is not added to the catalog.",
          entry._nodeBrowseName.c_str(), UA_StatusCode_name(retval));
      return;
    }
    else {
      if(accessLevel == (UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE))
        entry._isReadonly = false;
      else
        entry._isReadonly = true;
    }
    _catalogue_mutable.addRegister(entry);
  }

  void OpcUABackend::resetClient() {
    if(_subscriptionManager) {
      {
        std::lock_guard<std::mutex> lock(_connection->client_lock);
        _subscriptionManager->deactivate();
      }
      _subscriptionManager->resetMonitoredItems();
    }
    /*
     *  close connection
     *
     *  In the tests after unlocking the server the client reused the session that
     *  was active before the locking the server. In that case no initial value was sent.
     *  By closing the session we force an initial value to be send when reconnecting the
     *  client.
     */
    {
      std::lock_guard<std::mutex> lock(_connection->client_lock);
      _connection->close();
    }
  }

  void OpcUABackend::open() {
    /* Normally client is already connected in the constructor.
     * But open() is also called by ApplicationCore in case an error
     * was detected by the Backend (i.e. an exception was thrown
     * by one of the RegisterAccessors).
     */

    // if an error was seen by the accessor the error is in the queue but as long as no read happened setException is
    // not called but in the tests the device is reset without calling read in between -> so we need to check the
    // connection here
    // -> to make sure the Subscription internal thread is stopped.
    if(!isFunctional() || !_connection->isConnected()) {
      UA_LOG_DEBUG(
          UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Opening the device: %s", _connection->serverAddress.c_str());
      if(_subscriptionManager) {
        if(_subscriptionManager->_opcuaThread && _subscriptionManager->_opcuaThread->joinable()) {
          _subscriptionManager->_opcuaThread->join();
          _subscriptionManager->_opcuaThread.reset(nullptr);
        }
      }
      connect();
    }

    if(!_catalogue_filled) {
      fillCatalogue();
      _catalogue_filled = true;
    }
    _opened = true;
    // wait at maximum 100ms for the client to come up
    uint i = 0;
    while(!_connection->isConnected()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      ++i;
      if(i > 4) throw ChimeraTK::runtime_error("Connection could not be established.");
    }
    if(!_isFunctional) {
      UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
          "open() was called but client connection was still up. Maybe setException() was called manually.");
      _isFunctional = true;
    }
  }

  void OpcUABackend::close() {
    _opened = false;
    _isFunctional = false;
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Closing the device: %s", _connection->serverAddress.c_str());
    resetClient();
    //\ToDo: Check if we should reset the catalogue after closing. The UnifiedBackendTest will fail in that case.
    //    _catalogue_mutable = RegisterCatalogue();
    //    _catalogue_filled = false;
    _connection->close();
  }

  void OpcUABackend::connect() {
    resetClient();
    UA_StatusCode retval;
    {
      std::lock_guard<std::mutex> lock(_connection->client_lock);
      /** Connect **/
      if(_connection->username.empty() || _connection->password.empty()) {
        retval = UA_Client_connect(_connection->client.get(), _connection->serverAddress.c_str());
      }
      else {
        retval = UA_Client_connectUsername(_connection->client.get(), _connection->serverAddress.c_str(),
            _connection->username.c_str(), _connection->password.c_str());
      }
    }
    if(retval != UA_STATUSCODE_GOOD) {
      std::stringstream ss;
      ss << "Failed to connect to opc server: " << _connection->serverAddress
         << " with reason: " << UA_StatusCode_name(retval);
      throw ChimeraTK::runtime_error(ss.str());
    }
    else {
      UA_LOG_INFO(
          UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Connection established:  %s ", _connection->serverAddress.c_str());
    }
    // if already setup subscriptions where used
    if(_subscriptionManager) _subscriptionManager->prepare();
  }

  void OpcUABackend::activateAsyncRead() noexcept {
    std::lock_guard<std::mutex> lock(_asyncReadLock);
    if(!_opened || !_isFunctional) return;
    if(!_subscriptionManager) _subscriptionManager = std::make_unique<OPCUASubscriptionManager>(_connection);
    _subscriptionManager->activate();

    // Check if thread is already running-> happens if a second logicalNameMappingBackend is calling activateAsyncRead
    // after a first one called activateAsyncRead already
    if(_subscriptionManager->_opcuaThread == nullptr) {
      _subscriptionManager->start();
      // sleep twice the publishing interval to make sure intital values are written
      std::this_thread::sleep_for(std::chrono::milliseconds(2 * _connection->publishingInterval));
    }
  }

  void OpcUABackend::activateSubscriptionSupport() {
    if(!_subscriptionManager) _subscriptionManager = std::make_unique<OPCUASubscriptionManager>(_connection);
  }

  void OpcUABackend::setExceptionImpl() noexcept {
    std::lock_guard<std::mutex> lock(_connection->client_lock);
    if(_subscriptionManager) _subscriptionManager->deactivateAllAndPushException();
  }

  template<typename UserType>
  boost::shared_ptr<NDRegisterAccessor<UserType>> OpcUABackend::getRegisterAccessor_impl(
      const RegisterPath& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags) {
    std::string path = _connection->serverAddress + registerPathName;

    if(_catalogue_mutable.getNumberOfRegisters() == 0) {
      throw ChimeraTK::logic_error("No registers found in the catalog.");
    }

    OpcUABackendRegisterInfo* info = nullptr;
    for(auto it = _catalogue_mutable.begin(), ite = _catalogue_mutable.end(); it != ite; it++) {
      if(it->getRegisterName() == registerPathName) {
        info = dynamic_cast<OpcUABackendRegisterInfo*>(&(*it));
        break;
      }
    }
    if(info == nullptr) {
      throw ChimeraTK::logic_error(
          std::string("Requested register (") + registerPathName + ") was not found in the catalog.");
    }

    if(numberOfWords + wordOffsetInRegister > info->_arrayLength || (numberOfWords == 0 && wordOffsetInRegister > 0)) {
      std::stringstream ss;
      ss << "Requested number of words/elements ( " << numberOfWords << ") with offset " << wordOffsetInRegister
         << " exceeds the number of available words/elements: " << info->_arrayLength;
      throw ChimeraTK::logic_error(ss.str());
    }

    if(numberOfWords == 0) numberOfWords = info->_arrayLength;

    switch(info->_dataType) {
      case 1:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_Boolean, UserType>>(
            path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 2:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_SByte, UserType>>(
            path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 3:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_Byte, UserType>>(
            path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 4:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_Int16, UserType>>(
            path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 5:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_UInt16, UserType>>(
            path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 6:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_Int32, UserType>>(
            path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 7:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_UInt32, UserType>>(
            path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 8:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_Int64, UserType>>(
            path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 9:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_UInt64, UserType>>(
            path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 10:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_Float, UserType>>(
            path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 11:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_Double, UserType>>(
            path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      case 12:
        return boost::make_shared<OpcUABackendRegisterAccessor<UA_String, UserType>>(
            path, shared_from_this(), registerPathName, info, flags, numberOfWords, wordOffsetInRegister);
        break;
      default:
        throw ChimeraTK::runtime_error(std::string("Type ") + std::to_string(info->_dataType) + " not implemented.");
        break;
    }
  }

  OpcUABackend::BackendRegisterer::BackendRegisterer() {
    BackendFactory::getInstance().registerBackendType("opcua", &OpcUABackend::createInstance,
        {"port", "username", "password", "map", "publishingInterval", "rootNode", "connectionTimeout"});
    std::cout << "BackendRegisterer: registered backend type opcua" << std::endl;
  }

  RegisterCatalogue OpcUABackend::getRegisterCatalogue() const {
    return RegisterCatalogue(_catalogue_mutable.clone());
  }

  boost::shared_ptr<DeviceBackend> OpcUABackend::createInstance(
      std::string address, std::map<std::string, std::string> parameters) {
    if(parameters["port"].empty()) {
      throw ChimeraTK::logic_error("Missing OPC-UA port.");
    }

    std::string serverAddress = std::string("opc.tcp://") + address + ":" + parameters["port"];
    unsigned long publishingInterval = 500;
    if(!parameters["publishingInterval"].empty()) publishingInterval = std::stoul(parameters["publishingInterval"]);

    ulong rootNS;
    std::string rootName("");
    if(parameters["map"].empty()) {
      if(!parameters["rootNode"].empty()) {
        // prepare automatic browsing
        auto pos = parameters["rootNode"].find_first_of(":");
        if(pos == std::string::npos) {
          throw ChimeraTK::runtime_error(
              "root node does not contain delimter ':' formated correct. Expected ns:nodeid or ns:nodename!");
        }
        try {
          rootNS = std::stoul(parameters["rootNode"].substr(0, pos));
        }
        catch(...) {
          throw ChimeraTK::runtime_error("failed to determine ns from root node");
        }
        rootName = parameters["rootNode"].substr(pos + 1);
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
            "Set root name for automatic browsing to: %s. Name space: %ld", rootName.c_str(), rootNS);
      }
    }
    else {
      // prepare map file based browsing
      if(!parameters["rootNode"].empty()) {
        rootName = parameters["rootNode"];
      }
    }
    long int connetionTimeout = 5000;
    if(!parameters["connectionTimeout"].empty()) {
      connetionTimeout = std::stoi(parameters["connectionTimeout"]);
    }
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Connection timeout is set to: %ld ms", connetionTimeout);

    return boost::shared_ptr<DeviceBackend>(new OpcUABackend(serverAddress, parameters["username"],
        parameters["password"], parameters["map"], publishingInterval, rootName, rootNS, connetionTimeout));
  }
} // namespace ChimeraTK
