/*
 * SubscriptionManager.h
 *
 *  Created on: Dec 17, 2020
 *      Author: Klaus Zenker (HZDR)
 */

#ifndef INCLUDE_SUBSCRIPTIONMANAGER_H_
#define INCLUDE_SUBSCRIPTIONMANAGER_H_

#include <map>
#include <vector>
#include <mutex>
#include <thread>
#include "open62541.h"
#include <iostream>

namespace ChimeraTK{
  class OpcUABackendRegisterAccessorBase;

  /**
   * Struct used to store all information about a backend subscription, which is a monitored item belonging to a OPC UA subscription in terms of OPC UA.
   */
  struct MonitorItem{
    UA_NodeId node; ///< Node id of the process variable to be monitored
    OpcUABackendRegisterAccessorBase* accessor; ///< Pointer to the accessor using this item
    UA_UInt32 id; ///< ID of the monitored item that belongs to the subscription
    bool active; ///< If active the data is updated by the callback function
  };


  /**
   * Class handling the OPC UA subscriptions and monitored items.
   *
   * So far only one subscription is used and all ctk subscriptions of BackendAccessors are monitored items to this subscription.
   */
  class OPCUASubscriptionManager{
  public:
    static OPCUASubscriptionManager& getInstance() {
      static OPCUASubscriptionManager manager;
      return manager;
    }

    void activate();

    void deactivate();

//    template<typename UAType, typename CTKType>
//    void subscribe(const UA_NodeId& id, OpcUABackendRegisterAccessor<UAType, CTKType>* accessor);

    /**
     * The subscription is done before the device is opened, when the Accessor is created.
     * At this point the client is not yet set up and the registration of the monitored item can not yet be done.
     * This will be done in addMonitoredItems() when the device is opened.
     */
    void subscribe(const UA_NodeId& node, OpcUABackendRegisterAccessorBase* accessor);

    void unsubscribe(const UA_UInt32& id);

    void start();

    void runClient();

    void setClient(UA_Client* client, std::mutex* opcuaMutex);

    /**
     * Check if client is up and subscriptions are valid
     */
    bool isActive();

    /// callback function for the client
//    template <typename UAType>
    static void responseHandler(UA_UInt32 monId, UA_DataValue *value, void *monContext);

  private:
    OPCUASubscriptionManager(){};
    ~OPCUASubscriptionManager();

    /**
     * Here the items are registered to the server by the client.
     * This is to be called after the client is set up.
     * It is called by setClient()
     */
    void addMonitoredItems();


    bool _run{false};
    bool _subscriptionActive{false};

    UA_Client* _client{nullptr};
    std::mutex* _opcuaMutex{nullptr};

    UA_UInt32 _subscriptionID;

    /*
     *  To keep asynchronous services alive (e.g. renew secure channel,...) the client needs
     *  to call UA_run_iterate all the time, which is done in this thread.
     */
    std::thread _opcuaThread;

    // List of subscriptions (not OPC UA subscriptions)
    std::vector<MonitorItem> _items;

    /// map of ctk subscriptions (not OPC UA subscriptions)
    static std::map<UA_UInt32, MonitorItem*> subscriptionMap;

    // Send exception to all accesors via the future queue
    void handleException();
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
