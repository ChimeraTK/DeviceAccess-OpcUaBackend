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


OPCUASubscriptionManager::~OPCUASubscriptionManager(){
  deactivate();
}


void OPCUASubscriptionManager::start(){
  if(subscriptionMap.size() != 0){
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
      if(state != UA_CONNECTION_ESTABLISHED)
        break;

    }
    if(ret != UA_STATUSCODE_GOOD)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
              "Stopped sending publish requests.");
  //Inform all accessors that are subscribed
  handleException();
  _run = false;
}

void OPCUASubscriptionManager::activate(){
  for(auto &item : OPCUASubscriptionManager::subscriptionMap){
    item.second->active = true;
  }
}

void OPCUASubscriptionManager::deactivate(){
  // check if subscription thread was started at all. If yes _run was set true in OPCUASubscriptionManager::start()
  for(auto &item : OPCUASubscriptionManager::subscriptionMap){
    item.second->active = false;
  }
  if(_run == true){
    _run = false;
    if(_opcuaThread.joinable())
      _opcuaThread.join();
  }
  if(_subscriptionActive){
    unsubscribe(_subscriptionID);
    _subscriptionActive = false;
  }
  _client = nullptr;
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

  if(OPCUASubscriptionManager::subscriptionMap[monId]->active){
    OPCUASubscriptionManager::subscriptionMap[monId]->accessor->_notifications.push_overwrite(data);
  }
}


void OPCUASubscriptionManager::addMonitoredItems(){
  if(_client == nullptr){
    throw ChimeraTK::logic_error("No client pointer available in runClient.");
  }
  std::lock_guard<std::mutex> lock(*_opcuaMutex);
  if(!_subscriptionActive){
    /* Create the subscription with default configuration. */

    UA_StatusCode retval = UA_Client_Subscriptions_new(_client, UA_SubscriptionSettings_standard, &_subscriptionID);
    if(retval == UA_STATUSCODE_GOOD){
      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
        "Create subscription succeeded, id %u", _subscriptionID);
      _subscriptionActive = true;
    } else {
      throw ChimeraTK::runtime_error("Failed to set up subscription.");
    }
  }

  for(auto &item : _items){
    // create monitored item
    UA_StatusCode retval = UA_Client_Subscriptions_addMonitoredItem(_client,_subscriptionID, item.node,UA_ATTRIBUTEID_VALUE, &responseHandler, NULL,&item.id);

    /* Check server response to adding the item to be monitored. */
    if(retval == UA_STATUSCODE_GOOD){
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
          "Monitoring id %u", item.id);
        OPCUASubscriptionManager::subscriptionMap[item.id] = &item;
    } else {
      throw ChimeraTK::runtime_error("Failed to add monitored item for node: " + item.accessor->_info->getRegisterPath());
    }
  }
}

void  OPCUASubscriptionManager::subscribe(const UA_NodeId& node, OpcUABackendRegisterAccessorBase* accessor){
  /* Request monitoring for the node of interest. */
  MonitorItem item;
  item.id = 0;
  item.accessor = accessor;
  item.node = node;
  item.active = false;

  _items.push_back(item);

}

void OPCUASubscriptionManager::setClient(UA_Client* client, std::mutex* opcuaMutex){
  _subscriptionActive = false;
  _run = false;
  if(_opcuaThread.joinable())
    _opcuaThread.join();
  OPCUASubscriptionManager::subscriptionMap.clear();
  // set new client
  _client = client;
  _opcuaMutex = opcuaMutex;
  addMonitoredItems();
  _run = true;
}

void OPCUASubscriptionManager::unsubscribe(const UA_UInt32& id){
  if(_client == nullptr){
    throw ChimeraTK::logic_error("No client pointer available in runClient.");
  }
  std::lock_guard<std::mutex> lock(*_opcuaMutex);
  if(!UA_Client_Subscriptions_remove(_client, id)){
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
        "Subscriptions sucessfully removed.");
  }
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

void OPCUASubscriptionManager::handleException(){
  for(auto &item : OPCUASubscriptionManager::subscriptionMap){
    try {
      throw ChimeraTK::runtime_error("OPC UA connection lost.");
    } catch(...) {
      item.second->accessor->_notifications.push_overwrite_exception(std::current_exception());
    }
  }
}

}
