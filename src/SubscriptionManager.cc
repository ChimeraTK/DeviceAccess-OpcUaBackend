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

#include <open62541/client_subscriptions.h>
#include <open62541/client.h>

namespace ChimeraTK{

OPCUASubscriptionManager::~OPCUASubscriptionManager(){
  // already called when closing the device...
  deactivate();
  if(_opcuaThread && _opcuaThread->joinable()){
    _opcuaThread->join();
    _opcuaThread.reset(nullptr);
  }
}


void OPCUASubscriptionManager::start(){
  //\ToDo: In fact this thread will always run since the subscription is always activated by the backend (Needed for adding subscription after device open and resynActivate).
  if(_subscriptionActive){
    _run = true;
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "Starting subscription thread with publishing interval of %ldms.", _connection->publishingInterval);
    _opcuaThread.reset(new std::thread{&OPCUASubscriptionManager::runClient,this});
  } else {
    _run = false;
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "No active subscriptions. No need to start the subscription manager.");
  }
}

void OPCUASubscriptionManager::runClient(){
  UA_StatusCode ret;
  UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Starting client iterate loop.");
  uint64_t i = 0;
  while(_run){
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "Sending subscription request.");
    {
      std::lock_guard<std::mutex> lock(_connection->client_lock);
      ret = UA_Client_run_iterate(_connection->client.get(),0);
    }
    if(ret != UA_STATUSCODE_GOOD){
      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "Stopped sending publish requests. OPC UA message: %s", UA_StatusCode_name(ret));

      break;
    }
    // sleep for half the publishing interval - should be fine to catch up all updates
    std::this_thread::sleep_for(std::chrono::milliseconds(_connection->publishingInterval/2));
    ++i;
    if(i%50 == 0)
      UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                          "Still running client iterate loop.");
  }
  UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Stopped client iterate loop.");

  //Inform all accessors that are subscribed
//  if(_run)
//    handleException("OPC UA connection lost.");
  _run = false;
}

void OPCUASubscriptionManager::activate(){
  if(!_subscriptionActive)
    createSubscription();
  _asyncReadActive = true;
  if(_items.size() > 0 && subscriptionMap.size() == 0){
    addMonitoredItems();
  } else {
    std::lock_guard<std::mutex> lock(_mutex);
    for(auto &item : _items){
      item._active = true;
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

  if(_subscriptionActive){
    _subscriptionActive = false;
    // lock is hold in runClient and this method is called in the state callback
//    std::lock_guard<std::mutex> lock(_connection->client_lock);
    removeSubscription();
  }
  _asyncReadActive = false;
}

void OPCUASubscriptionManager::removeSubscription(){
  if(!UA_Client_Subscriptions_deleteSingle(_connection->client.get(), _subscriptionID)){
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
        "Sucessfully removed old subscription.");
  }

}

void OPCUASubscriptionManager::deactivateAllAndPushException(std::string message){
  handleException(message);
  deactivate();
}

void OPCUASubscriptionManager::responseHandler(UA_Client *client, UA_UInt32 subId, void *subContext,
    UA_UInt32 monId, void *monContext, UA_DataValue *value){
  UA_DateTime sourceTime = value->sourceTimestamp;
  UA_DateTimeStruct dts = UA_DateTime_toStruct(sourceTime);
  UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
  "Subscription handler called.\nSource time stamp: %02u-%02u-%04u %02u:%02u:%02u.%03u, ",
                 dts.day, dts.month, dts.year, dts.hour, dts.min, dts.sec, dts.milliSec);
  auto base = reinterpret_cast<OPCUASubscriptionManager*>(monContext);

  try{
    if(base->subscriptionMap.at(monId)->_active){
      // only lock the mutex if active. This is used when unsubscribing to avoid dead locks
      std::lock_guard<std::mutex> lock(base->_mutex);
      base->subscriptionMap[monId]->_hasException = false;
      for(auto &accessor : base->subscriptionMap[monId]->_accessors){
			  UA_DataValue data;
			  UA_DataValue_init(&data);
			  UA_DataValue_copy(value, &data);
        accessor->_notifications.push_overwrite(std::move(data));
      }
    }
  } catch (std::out_of_range &e){
    // When calling unsubscribe the item is removed before it is unsubscribed from the client, which might trigger the response handler.
    UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
      "Responsehandler for monitored item with id %d called but item is already removed.", subId);
  }
}

void OPCUASubscriptionManager::createSubscription(){
  /* Create the subscription with default configuration. */
  UA_CreateSubscriptionRequest request = UA_CreateSubscriptionRequest_default();
  request.requestedPublishingInterval = _connection->publishingInterval;
  UA_CreateSubscriptionResponse response;
  {
    std::lock_guard<std::mutex> lock(_connection->client_lock);
    response = UA_Client_Subscriptions_create(_connection->client.get(), request, NULL, NULL, deleteSubscriptionCallback);
  }
  if(response.responseHeader.serviceResult == UA_STATUSCODE_GOOD){
    _subscriptionID = response.subscriptionId;
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
      "Create subscription succeeded, id %u", _subscriptionID);
    if(response.revisedPublishingInterval != _connection->publishingInterval){
      UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
            "Publishing interval was changed from %ldms to %fms", _connection->publishingInterval, response.revisedPublishingInterval);
      // replace publishing interval with revised publishing interval as it will be used to set the sampling interval of monitored items
      _connection->publishingInterval = response.revisedPublishingInterval;
    }
    _subscriptionActive = true;
  } else {
    throw ChimeraTK::runtime_error("Failed to set up subscription.");
  }
}

void OPCUASubscriptionManager::addMonitoredItems(){
  _mutex.lock();
  for(auto &item : _items){
    if(_asyncReadActive)
      item._active = true;
    if(!item._isMonitored){
      // create monitored item
      // pass object as context to the callback function. This allows to use individual subscriptionMaps for each manager!
      UA_MonitoredItemCreateRequest monRequest = UA_MonitoredItemCreateRequest_default(item._node);
      // sampling interval equal to the publishing interval set for the subscription
      monRequest.requestedParameters.samplingInterval = _connection->publishingInterval;
      UA_MonitoredItemCreateResult monResponse;
      // unlock mutex because the OPC UA call potentially ends up in a state callback which enters deactivateAllAndPushException
      _mutex.unlock();
      {
        std::lock_guard<std::mutex> lock(_connection->client_lock);
        monResponse = UA_Client_MonitoredItems_createDataChange(_connection->client.get(),
          _subscriptionID, UA_TIMESTAMPSTORETURN_BOTH, monRequest,
          this, &OPCUASubscriptionManager::responseHandler, NULL);
      }
      _mutex.lock();
      /* Check server response to adding the item to be monitored. */
      if(monResponse.statusCode == UA_STATUSCODE_GOOD){
        item._id = monResponse.monitoredItemId;
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
            "Monitoring id %u (%s) for pv: %s", item._id, _connection->serverAddress.c_str(), item._browseName.c_str());
        if(monResponse.revisedSamplingInterval != _connection->publishingInterval){
          UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                      "Publishing interval was changed from %ldms to %fms", _connection->publishingInterval, monResponse.revisedSamplingInterval);
        }
        subscriptionMap[item._id] = &item;
        item._isMonitored = true;
      } else {
        _mutex.unlock();
        handleException(std::string("Failed to add monitored item for node: ") + item._browseName + " Error: " + UA_StatusCode_name(monResponse.statusCode));
        _mutex.lock();
      }
    }
  }
  _mutex.unlock();
}

void OPCUASubscriptionManager::prepare(){
  createSubscription();
  addMonitoredItems();
}

void  OPCUASubscriptionManager::subscribe(const std::string &browseName, const UA_NodeId& node, OpcUABackendRegisterAccessorBase* accessor){
  //\ToDo: Check if already monitored based on node using the NodeStore?!
  std::deque<MonitorItem>::iterator it;
  _mutex.lock();
  it = std::find(_items.begin(), _items.end(), browseName);

  if(it == _items.end()){
    /* Request monitoring for the node of interest. */
    MonitorItem item(browseName, node, accessor);
    _items.push_back(item);

    _mutex.unlock();
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
      std::lock_guard<std::mutex> lock(tmp->_dataUpdateLock);
      if(!UA_Variant_isEmpty(&tmp->_data.value)){
        UA_DataValue data;
        UA_DataValue_init(&data);
        UA_DataValue_copy(&tmp->_data, &data);
        accessor->_notifications.push_overwrite(std::move(data));
      }
    }
    _mutex.unlock();
  }
}

OPCUASubscriptionManager::OPCUASubscriptionManager(std::shared_ptr<OPCUAConnection> connection):
  _connection(connection), _subscriptionActive(false), _asyncReadActive(false), _run(true), _subscriptionID(0){
}

void OPCUASubscriptionManager::resetMonitoredItems(){
  std::lock_guard<std::mutex> lock(_mutex);
  if(_opcuaThread && _opcuaThread->joinable()){
    _opcuaThread->join();
    _opcuaThread.reset(nullptr);
  }

  for(auto &item : _items){
    item._isMonitored = false;
  }
  subscriptionMap.clear();
}

void OPCUASubscriptionManager::removeMonitoredItems(){
  std::lock_guard<std::mutex> lock(_mutex);
  _items.clear();
}

void OPCUASubscriptionManager::unsubscribe(const std::string &browseName, OpcUABackendRegisterAccessorBase* accessor){
  // If the id is set an item is to be removed from the client. Before the _mutex lock is released.
  UA_UInt32 id{0};
  {
    std::lock_guard<std::mutex> item_lock(_mutex);
    // client pointer might be reset already when closing the device - in this case nothing to do here
    auto it = std::find(_items.begin(), _items.end(), browseName);
    // in case the asyncread was activated but no variables were subscribed - unsubscribe is called by RegisterAccessor destructor
    if(it == _items.end())
      return;
    if(it->_accessors.size() > 1){
      // only remove accessor if still other accessors are using that subscription
      it->_accessors.erase(std::find(it->_accessors.begin(), it->_accessors.end(), accessor));
    } else {
      // remove monitored item
      subscriptionMap.erase(it->_id);
      id = it->_id;
      _items.erase(it);
    }
  }
  // try to unsubscribe
  if(id != 0 && _connection->isConnected()){
    UA_StatusCode ret;
    {
      std::lock_guard<std::mutex> connection_lock(_connection->client_lock);
      // UA_Client_MonitoredItems_deleteSingle tries to update the latest value which triggers responseHandler
      ret = UA_Client_MonitoredItems_deleteSingle(_connection->client.get(), _subscriptionID, id);
    }

    if(!ret){
      UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
              "Monitored item removed for: %s", browseName.c_str());
    } else {
      UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                        "Failed to unsubscribe item (no client alive): %s Error: %s", browseName.c_str(), UA_StatusCode_name(ret));
    }
  }
}

void OPCUASubscriptionManager::handleException(const std::string &message){
  std::lock_guard<std::mutex> lock(_mutex);
  UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                             "Handling error: %s", message.c_str());
  for(auto &item : _items){
    try {
      throw ChimeraTK::runtime_error(message);
    } catch(...) {
      if(item._active && !item._hasException){
        item._hasException = true;
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                           "Sending exception to %lu accessors for node %s.", item._accessors.size(), item._browseName.c_str());
        for(auto &accessor : item._accessors){
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
