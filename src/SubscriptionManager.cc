/*
 * SubscriptionManager.cc
 *
 *  Created on: Dec 17, 2020
 *      Author: Klaus Zenker (HZDR)
 */

#include <ChimeraTK/Exception.h>
#include "SubscriptionManager.h"
#include <chrono>
#include "OPC-UA-BackendRegisterAccessor.h"

namespace ChimeraTK{

std::map<UA_UInt32, MonitorItem*> OPCUASubscriptionManager::subscriptionMap;
std::mutex OPCUASubscriptionManager::_mutex;


OPCUASubscriptionManager::~OPCUASubscriptionManager(){
  deactivate();
}


void OPCUASubscriptionManager::start(){
  //\ToDo: In fact this thread will always run since the subscription is always activated by the backend (Needed for adding subscription after device open and resynActivate).
  if(_subscriptionActive){
    _run = true;
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "Starting subscription thread.");
    _opcuaThread = std::thread{&OPCUASubscriptionManager::runClient,this};
  } else {
    _run = false;
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "No active subscriptions. No need to start the subscription manager.");
  }
}

void OPCUASubscriptionManager::runClient(){
  if(_client == nullptr){
//    throw ChimeraTK::logic_error("No client pointer available in runClient.");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "No client pointer available in runClient.");
    _run = false;
    return;
  }
  UA_StatusCode ret;
  while(_run){
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "Sending subscription request.");
    {
      std::lock_guard<std::mutex> lock(*_opcuaMutex);
      ret = UA_Client_Subscriptions_manuallySendPublishRequest(_client);
      auto state = UA_Client_getConnectionState(_client);
      // \ToDo: Check also subscription state!
      if(state != UA_CONNECTION_ESTABLISHED){
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Stopped sending publish requests. Connection state: %u ", state);
        break;
      }
    }
    if(ret != UA_STATUSCODE_GOOD){
      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "Stopped sending publish requests. OPC UA message: %s", UA_StatusCode_name(ret));

      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  //Inform all accessors that are subscribed
  if(_run)
    handleException("OPC UA connection lost.");
  _run = false;
}

void OPCUASubscriptionManager::activate(){
  std::lock_guard<std::mutex> lock(_mutex);
  _asyncReadActive = true;
  if(_items.size() > 0 && subscriptionMap.size() == 0){
    addMonitoredItems();
  } else {
    for(auto &item : subscriptionMap){
      item.second->_active = true;
    }
  }
}

void OPCUASubscriptionManager::deactivate(bool keepItems){
  // check if subscription thread was started at all. If yes _run was set true in OPCUASubscriptionManager::start()
  std::cout << "Calling deactivate with keepItems: " << keepItems << std::endl;
  std::lock_guard<std::mutex> lock(_mutex);
  for(auto &item : subscriptionMap){
    item.second->_active = false;
  }
  if(_run == true){
    _run = false;
    if(_opcuaThread.joinable())
      _opcuaThread.join();
  }
  if(_subscriptionActive){
    std::lock_guard<std::mutex> lock(*_opcuaMutex);
    if(!UA_Client_Subscriptions_remove(_client, _subscriptionID)){
      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
          "Subscriptions sucessfully removed.");
    }
    _subscriptionActive = false;
  }
  if(!keepItems)
    OPCUASubscriptionManager::_items.clear();
  _client = nullptr;
  _asyncReadActive = false;
}

void OPCUASubscriptionManager::deactivateAllAndPushException(){
  handleException("Exception reported by another accessor.");
  deactivate(true);
}

void OPCUASubscriptionManager::responseHandler(UA_UInt32 monId, UA_DataValue *value, void *monContext){
  std::cout << "Subscription Handler called." << std::endl;
//    UAType* result = (UAType*)value->value.data;
  UA_DateTime sourceTime = value->sourceTimestamp;
  UA_DateTimeStruct dts = UA_DateTime_toStruct(sourceTime);
//    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
//    "Monitored Item ID: %d value: %f.2", monId,*result);
  UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
  "Source time stamp: %02u-%02u-%04u %02u:%02u:%02u.%03u, ",
                 dts.day, dts.month, dts.year, dts.hour, dts.min, dts.sec, dts.milliSec);
  UA_DataValue data;
  UA_DataValue_copy(value, &data);

  // only for debugging!!
  UA_Int32* tmp = (UA_Int32*)(data.value.data);
  UA_Int32 tmpVal = tmp[0];
  UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
    "Data: %02u", tmpVal);
  std::lock_guard<std::mutex> lock(_mutex);
  if(subscriptionMap[monId]->_active){
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
      "Pushing data to the queue.");
    subscriptionMap[monId]->_hasException = false;
    for(auto &accessor : subscriptionMap[monId]->_accessors){
      accessor->_notifications.push_overwrite(data);
    }
  }
}

void OPCUASubscriptionManager::createSubscription(){
  if(_client == nullptr){
    throw ChimeraTK::logic_error("No client pointer available in runClient.");
  }
  std::lock_guard<std::mutex> lock(*_opcuaMutex);
  if(!_subscriptionActive){
    /* Create the subscription with default configuration. */

    auto config = UA_SubscriptionSettings_standard;
    config.requestedPublishingInterval = _publishingInterval;
    UA_StatusCode retval = UA_Client_Subscriptions_new(_client, config, &_subscriptionID);
    if(retval == UA_STATUSCODE_GOOD){
      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
        "Create subscription succeeded, id %u", _subscriptionID);
      _subscriptionActive = true;
    } else {
      throw ChimeraTK::runtime_error("Failed to set up subscription.");
    }
  }
}

void OPCUASubscriptionManager::addMonitoredItems(){
  if(_client == nullptr){
    throw ChimeraTK::logic_error("No client pointer available in runClient.");
  }
  for(auto &item : _items){
    if(_asyncReadActive)
      item._active = true;
    if(!item._isMonitored){
      // create monitored item
      std::lock_guard<std::mutex> lock(*_opcuaMutex);
      UA_StatusCode retval = UA_Client_Subscriptions_addMonitoredItem(_client,_subscriptionID, item._node,UA_ATTRIBUTEID_VALUE, &responseHandler, NULL,&item._id);

      /* Check server response to adding the item to be monitored. */
      if(retval == UA_STATUSCODE_GOOD){
          UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
            "Monitoring id %u", item._id);
          subscriptionMap[item._id] = &item;
          item._isMonitored = true;
      } else {
        handleException("Failed to add monitored item for node: " + (*item._accessors.begin())->_info->getRegisterPath());
      }
    }
  }
}

void  OPCUASubscriptionManager::subscribe(const std::string &browseName, const UA_NodeId& node, OpcUABackendRegisterAccessorBase* accessor){
  //\ToDo: Check if already monitored based on node using the NodeStore?!
  auto it = std::find(_items.begin(), _items.end(), browseName);
  std::lock_guard<std::mutex> lock(_mutex);
  if(it == _items.end()){
    /* Request monitoring for the node of interest. */
    MonitorItem item(browseName, node, accessor);

    _items.push_back(item);

    // check if device was already opened
    if(_subscriptionActive){
      addMonitoredItems();
    }
  } else {
    std::cout << "Adding accessor to existing node subscription." << std::endl;
    auto tmp = it->_accessors.back();
    it->_accessors.push_back(accessor);
    if(it->_active){
      // if already active add initial value
      std::cout << "Setting intial value for accessor with existing node subscription." << std::endl;
      accessor->_notifications.push_overwrite(tmp->_data);
    }
  }
}

void OPCUASubscriptionManager::setClient(UA_Client* client, std::mutex* opcuaMutex, const unsigned long &publishingInterval){
  _subscriptionActive = false;
  _asyncReadActive = false;
  _run = false;
  if(_opcuaThread.joinable())
    _opcuaThread.join();
  resetMonitoredItems();
  // set new client
  _publishingInterval = publishingInterval;
  _client = client;
  _opcuaMutex = opcuaMutex;
  createSubscription();
//  addMonitoredItems();
  _run = true;
}

void OPCUASubscriptionManager::resetMonitoredItems(){
  std::lock_guard<std::mutex> lock(_mutex);
  for(auto &item : _items){
    item._isMonitored = false;
  }
  subscriptionMap.clear();
}

void OPCUASubscriptionManager::unsubscribe(const std::string &browseName, OpcUABackendRegisterAccessorBase* accessor){
  if(_client != nullptr){
    // client pointer might be reset already when closing the device - in this case nothing to do here
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = std::find(_items.begin(), _items.end(), browseName);
    if(it->_accessors.size() > 1){
      // only remove accessor if still other accessors are using that subscription
      it->_accessors.erase(std::find(it->_accessors.begin(), it->_accessors.end(), accessor));
    } else {
      // remove subscription
      // find map item
      for(auto it_map = subscriptionMap.begin(); it_map != subscriptionMap.end(); ++it){
        if(it_map->second->_browseName == browseName){
          std::lock_guard<std::mutex> lock(*_opcuaMutex);
          if(!UA_Client_Subscriptions_removeMonitoredItem(_client, _subscriptionID, it_map->first)){
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Monitored item removed for: %s", browseName.c_str());
            subscriptionMap.erase(it_map);
            _items.erase(it);
          } else {
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                              "Failed to unsubscribe item : %s", browseName.c_str());
          }
          break;
        }
      }
    }
  }

//  if(!UA_Client_Subscriptions_remove(_client, id)){
//    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
//        "Subscriptions sucessfully removed.");
//  }
//  OPCUASubscriptionManager::subscriptionMap.clear();
}

bool OPCUASubscriptionManager::isActive(){
  // If the subscriptions is broken the thread will set _run to false
//  return _run;
  std::cout << "Checking subscription active..." << std::endl;
  std::lock_guard<std::mutex> lock(*_opcuaMutex);
  // update client state
  UA_Client_Subscriptions_manuallySendPublishRequest(_client);
  // check connection
  auto connection = UA_Client_getConnectionState(_client);
  // \ToDo: Check also subscription state!
  if(connection != UA_CONNECTION_ESTABLISHED)
    return false;
  auto state = UA_Client_getState(_client);
  if(state != UA_CLIENTSTATE_CONNECTED)
    return false;
  std::cout << "Checking subscription active...is active!" << std::endl;
  return true;
}

void OPCUASubscriptionManager::handleException(const std::string &message){
  std::lock_guard<std::mutex> lock(_mutex);
  for(auto &item : subscriptionMap){
    try {
      throw ChimeraTK::runtime_error(message);
    } catch(...) {
      if(item.second->_active && !item.second->_hasException){
        item.second->_hasException = true;
        std::cout << "Sending exception to " << item.second->_accessors.size() << " accessors." << std::endl;
        for(auto &accessor : item.second->_accessors){
          accessor->_notifications.push_overwrite_exception(std::current_exception());
        }
      }
    }
  }
}

void OPCUASubscriptionManager::setExternalError(const std::string &browseName){
  auto it = std::find(_items.begin(), _items.end(), browseName);
  if(it != _items.end()){
    std::lock_guard<std::mutex> lock(_mutex);
    it->_hasException = true;
  }
}

}
