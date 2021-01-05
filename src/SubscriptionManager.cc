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

std::map<UA_UInt32, OpcUABackendRegisterAccessorBase*> OPCUASubscriptionManager::subscriptionMap;

void OPCUASubscriptionManager::start(){
  _run = true;
  _opcuaThread = std::thread{&OPCUASubscriptionManager::runClient,this};
}

void OPCUASubscriptionManager::runClient(){
  while(_run){
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "Sending subscription request.");
    UA_Client_Subscriptions_manuallySendPublishRequest(_client);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
              "Stopped sending publish requests.");
}

void OPCUASubscriptionManager::activate(UA_Client* client){
  /* Create the subscription with default configuration. */
  UA_StatusCode retval = UA_Client_Subscriptions_new(client, UA_SubscriptionSettings_standard, &_subscriptionID);
  if(retval == UA_STATUSCODE_GOOD){
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
      "Create subscription succeeded, id %u", _subscriptionID);
  } else {
    throw ChimeraTK::runtime_error("Failed to set up subscription.");
  }
  _client = client;
}

void OPCUASubscriptionManager::deactivate(){
  _run = false;
  unsubscribe(_subscriptionID);
  if(_opcuaThread.joinable())
    _opcuaThread.join();
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
  OPCUASubscriptionManager::subscriptionMap[monId]->_notifications.push_overwrite(*value);
}

void  OPCUASubscriptionManager::subscribe(const UA_NodeId& id, OpcUABackendRegisterAccessorBase* accessor){
  /* Request monitoring for the node of interest. */
  UA_UInt32 itemID = 0;
  UA_StatusCode retval = UA_Client_Subscriptions_addMonitoredItem(_client,_subscriptionID, id,UA_ATTRIBUTEID_VALUE, &responseHandler, NULL,&itemID);
  UA_String str;
//    UA_NodeId_print(&id, &str);
  /* Check server response to adding the item to be monitored. */
  if(retval == UA_STATUSCODE_GOOD){
//      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
//        "Monitoring '%s', id %u", str, itemID);
      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
        "Monitoring id %u", itemID);
      OPCUASubscriptionManager::subscriptionMap[itemID] = accessor;
  }
  /* The first publish request should return the initial value of the variable */
  UA_Client_Subscriptions_manuallySendPublishRequest(_client);
}

void OPCUASubscriptionManager::unsubscribe(const UA_UInt32& id){
  if(!UA_Client_Subscriptions_remove(_client, id)){
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
        "Subscriptions sucessfully removed.");
  }
}

}
