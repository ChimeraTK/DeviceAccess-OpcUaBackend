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

OPCUASubscriptionManager::~OPCUASubscriptionManager(){
  deactivate();
}


void OPCUASubscriptionManager::start(){
  //\ToDo: In fact this thread will always run since the subscription is always activated by the backend (Needed for adding subscription after device open and resynActivate).
  if(_subscriptionActive){
    _run = true;
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "Starting subscription thread.");
    int retry = 10;
    int i = 0;
    while(_opcuaThread){
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      i++;
      std::cout << "Waiting for the thread to finish..." << std::endl;
      if (i > retry)
        throw ChimeraTK::runtime_error("Thread is still running.");
    }
    _opcuaThread.reset(new std::thread{&OPCUASubscriptionManager::runClient,this});
  } else {
    _run = false;
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "No active subscriptions. No need to start the subscription manager.");
  }
}

void OPCUASubscriptionManager::runClient(){
  if(!_connection->client){
//    throw ChimeraTK::logic_error("No client pointer available in runClient.");
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "No client pointer available in runClient.");
    _run = false;
    return;
  }
  UA_StatusCode ret;
  while(_run){
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "Sending subscription request.");
    {
      std::lock_guard<std::mutex> lock(_connection->client_lock);
      if(!_connection->client){
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Client point was removed by the subscription manager.");
        _run = false;
        return;
      }
      ret = UA_Client_Subscriptions_manuallySendPublishRequest(_connection->client.get());
      auto state = UA_Client_getConnectionState(_connection->client.get());
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
    std::this_thread::sleep_for(std::chrono::milliseconds(_connection->publishingInterval));
  }
  //Inform all accessors that are subscribed
  if(_run)
    handleException("OPC UA connection lost.");
  _run = false;
}

void OPCUASubscriptionManager::activate(){
  std::lock_guard<std::mutex> lock(_mutex);
  if(!_subscriptionActive)
    createSubscription();
  _asyncReadActive = true;
  if(_items.size() > 0 && subscriptionMap.size() == 0){
    addMonitoredItems();
  } else {
    for(auto &item : subscriptionMap){
      item.second->_active = true;
    }
  }
}

void OPCUASubscriptionManager::deactivate(){
  // check if subscription thread was started at all. If yes _run was set true in OPCUASubscriptionManager::start()
  //\ToDo: can we use resetMonitoredItems here?
  {
    std::lock_guard<std::mutex> lock(_mutex);
    for(auto &item : subscriptionMap){
      item.second->_active = false;
    }
  }
  if(_run == true)
    _run = false;
  if(_opcuaThread && _opcuaThread->joinable()){
    _opcuaThread->join();
    _opcuaThread.reset(nullptr);
  }
  if(_subscriptionActive){
    // set active false first to protect other threads from using an empty client pointer
    _subscriptionActive = false;
    std::lock_guard<std::mutex> lock(_connection->client_lock);
    if(!_connection->client){
      UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Why the client is already reset?");
    } else {
      if(!UA_Client_Subscriptions_remove(_connection->client.get(), _subscriptionID)){
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
            "Subscriptions sucessfully removed.");
      }
    }
  }
  _asyncReadActive = false;
}

void OPCUASubscriptionManager::deactivateAllAndPushException(){
  handleException("Exception reported by another accessor.");
  deactivate();
}

void OPCUASubscriptionManager::responseHandler(UA_UInt32 monId, UA_DataValue *value, void *monContext){
  UA_DateTime sourceTime = value->sourceTimestamp;
  UA_DateTimeStruct dts = UA_DateTime_toStruct(sourceTime);
  UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
  "Subscription handler called.\nSource time stamp: %02u-%02u-%04u %02u:%02u:%02u.%03u, ",
                 dts.day, dts.month, dts.year, dts.hour, dts.min, dts.sec, dts.milliSec);
  UA_DataValue data;
  UA_DataValue_copy(value, &data);

  auto base = reinterpret_cast<OPCUASubscriptionManager*>(monContext);

  std::lock_guard<std::mutex> lock(base->_mutex);
  if(base->subscriptionMap[monId]->_active){
    base->subscriptionMap[monId]->_hasException = false;
    for(auto &accessor : base->subscriptionMap[monId]->_accessors){
      accessor->_notifications.push_overwrite(data);
    }
  }
}

void OPCUASubscriptionManager::createSubscription(){
  std::lock_guard<std::mutex> lock(_connection->client_lock);
  if(!_connection->client)
    return;
  /* Create the subscription with default configuration. */
  auto config = UA_SubscriptionSettings_standard;
  config.requestedPublishingInterval = _connection->publishingInterval;
  UA_StatusCode retval = UA_Client_Subscriptions_new(_connection->client.get(), config, &_subscriptionID);
  if(retval == UA_STATUSCODE_GOOD){
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
      "Create subscription succeeded, id %u", _subscriptionID);
    _subscriptionActive = true;
  } else {
    throw ChimeraTK::runtime_error("Failed to set up subscription.");
  }
}

void OPCUASubscriptionManager::addMonitoredItems(){
  std::lock_guard<std::mutex> lock(_connection->client_lock);
  if(!_connection->client){
    throw ChimeraTK::logic_error("No client pointer available in runClient.");
  }
  for(auto &item : _items){
    if(_asyncReadActive)
      item._active = true;
    if(!item._isMonitored){
      // create monitored item
      // pass object as context to the callback function. This allows to use individual subscriptionMaps for each manager!
      UA_StatusCode retval = UA_Client_Subscriptions_addMonitoredItem(_connection->client.get(),_subscriptionID,
          item._node,UA_ATTRIBUTEID_VALUE, &OPCUASubscriptionManager::responseHandler, this,&item._id);

      /* Check server response to adding the item to be monitored. */
      if(retval == UA_STATUSCODE_GOOD){
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
            "Monitoring id %u (%s) for pv: %s", item._id, _connection->serverAddress.c_str(), item._browseName.c_str());
          subscriptionMap[item._id] = &item;
          item._isMonitored = true;
      } else {
        handleException("Failed to add monitored item for node: " + (*item._accessors.begin())->_info->getRegisterPath());
      }
    }
  }
}

void OPCUASubscriptionManager::resetClient(){
  resetMonitoredItems();
  createSubscription();
  addMonitoredItems();
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
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Adding accessor to existing node subscription.");
    auto tmp = it->_accessors.back();
    it->_accessors.push_back(accessor);
    if(it->_active){
      // if already active add initial value
      UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                      "Setting intial value for accessor with existing node subscription.");
      accessor->_notifications.push_overwrite(tmp->_data);
    }
  }
}

OPCUASubscriptionManager::OPCUASubscriptionManager(std::shared_ptr<OPCUAConnection> connection):
  _connection(connection), _subscriptionActive(false), _asyncReadActive(false), _run(true), _subscriptionID(0){
  createSubscription();
}

void OPCUASubscriptionManager::resetMonitoredItems(){
  std::lock_guard<std::mutex> lock(_mutex);
  // stop updates
  if(_run == true)
    _run = false;
  if(_opcuaThread && _opcuaThread->joinable()){
    _opcuaThread->join();
    _opcuaThread.reset(nullptr);
  }

  for(auto &item : _items){
    item._isMonitored = false;
  }
  subscriptionMap.clear();
}

void OPCUASubscriptionManager::unsubscribe(const std::string &browseName, OpcUABackendRegisterAccessorBase* accessor){
  std::lock_guard<std::mutex> lock(_mutex);
  // client pointer might be reset already when closing the device - in this case nothing to do here
  auto it = std::find(_items.begin(), _items.end(), browseName);
  // in case the asyncread was activated but no variables were subscribed - unsubscribe is called by RegisterAccessor destructor
  if(it == _items.end())
    return;
  if(it->_accessors.size() > 1){
    // only remove accessor if still other accessors are using that subscription
    it->_accessors.erase(std::find(it->_accessors.begin(), it->_accessors.end(), accessor));
  } else {
    // remove subscription
    // find map item
    for(auto it_map = subscriptionMap.begin(); it_map != subscriptionMap.end(); ++it_map){
      if(it_map->second->_browseName == browseName){
        std::lock_guard<std::mutex> lock(_connection->client_lock);
        // try to unsubscribe
        if(_connection->client && !UA_Client_Subscriptions_removeMonitoredItem(_connection->client.get(), _subscriptionID, it_map->first)){
          UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "Monitored item removed for: %s", browseName.c_str());
        } else {
          UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                            "Failed to unsubscribe item (no client alive): %s", browseName.c_str());
        }
        subscriptionMap.erase(it_map);
        _items.erase(it);
        break;
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
  std::lock_guard<std::mutex> lock(_connection->client_lock);
  // update client state
  UA_Client_Subscriptions_manuallySendPublishRequest(_connection->client.get());
  // check connection
  auto connection = UA_Client_getConnectionState(_connection->client.get());
  // \ToDo: Check also subscription state!
  if(connection != UA_CONNECTION_ESTABLISHED)
    return false;
  auto state = UA_Client_getState(_connection->client.get());
  if(state != UA_CLIENTSTATE_CONNECTED)
    return false;
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
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                           "Sending exception to %u accessors for node %s", item.second->_accessors.size(), item.second->_browseName.c_str());
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
