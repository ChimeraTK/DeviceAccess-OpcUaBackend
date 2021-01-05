/*
 * SubscriptionManager.h
 *
 *  Created on: Dec 17, 2020
 *      Author: Klaus Zenker (HZDR)
 */

#ifndef INCLUDE_SUBSCRIPTIONMANAGER_H_
#define INCLUDE_SUBSCRIPTIONMANAGER_H_

#include <map>
#include <thread>
#include "open62541.h"
#include <iostream>

namespace ChimeraTK{
  class OpcUABackendRegisterAccessorBase;

  class OPCUASubscriptionManager{
  public:
    static OPCUASubscriptionManager& getInstance() {
      static OPCUASubscriptionManager manager;
      return manager;
    }

    void activate(UA_Client* client);

    void deactivate();

//    template<typename UAType, typename CTKType>
//    void subscribe(const UA_NodeId& id, OpcUABackendRegisterAccessor<UAType, CTKType>* accessor);
    void subscribe(const UA_NodeId& id, OpcUABackendRegisterAccessorBase* accessor);

    void unsubscribe(const UA_UInt32& id);

    void start();

    void runClient();


    /// callback function for the client
//    template <typename UAType>
    static void responseHandler(UA_UInt32 monId, UA_DataValue *value, void *monContext);

  private:
    OPCUASubscriptionManager();
    ~OPCUASubscriptionManager();

    bool _run;

    UA_Client* _client;

    UA_UInt32 _subscriptionID;

    /*
     *  To keep asynchronous services alive (e.g. renew secure channel,...) the client needs
     *  to call UA_run_iterate all the time, which is done in this thread.
     */
    std::thread _opcuaThread;

    /// map of subscriptions
    static std::map<UA_UInt32, OpcUABackendRegisterAccessorBase*> subscriptionMap;
  };

  /**
   * Get CTKType from map <NodeID, NDRegisterAccessor> and get
   * UAType from fusion map of UATypes <Tpye, UA_TYPENAME> -> see DummyServer.cc dummyMap
   * but without the string. The UA_TYPENAME should be returned by one of the
   * responseHandler arguments.
   */
//  template <typename UAType>
//  void OPCUASubscriptionManager::responseHandler(UA_UInt32 monId, UA_DataValue *value, void *monContext){
//    std::cout << "Subscription Handler called." << std::endl;
////    UAType* result = (UAType*)value->value.data;
//    UA_DateTime sourceTime = value->sourceTimestamp;
//    UA_DateTimeStruct dts = UA_DateTime_toStruct(sourceTime);
////    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
////    "Monitored Item ID: %d value: %f.2", monId,*result);
//    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
//    "Source time stamp: %02u-%02u-%04u %02u:%02u:%02u.%03u, ",
//                   dts.day, dts.month, dts.year, dts.hour, dts.min, dts.sec, dts.milliSec);
//  }


////  template<typename UAType, typename CTKType>
//  void  OPCUASubscriptionManager::subscribe(const UA_NodeId& id, OpcUABackendRegisterAccessor<UAType, CTKType>* accessor){
//    /* Request monitoring for the node of interest. */
//    UA_UInt32 itemID = 0;
//    UA_StatusCode retval = UA_Client_Subscriptions_addMonitoredItem(_client,_subscriptionID, id,UA_ATTRIBUTEID_VALUE, &responseHandler, NULL,&itemID);
//    UA_String str;
////    UA_NodeId_print(&id, &str);
//    /* Check server response to adding the item to be monitored. */
//    if(retval == UA_STATUSCODE_GOOD){
////      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
////        "Monitoring '%s', id %u", str, itemID);
//        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
//          "Monitoring id %u", itemID);
//
//    }
//    /* The first publish request should return the initial value of the variable */
//    UA_Client_Subscriptions_manuallySendPublishRequest(_client);
//  }
}



#endif /* INCLUDE_SUBSCRIPTIONMANAGER_H_ */
