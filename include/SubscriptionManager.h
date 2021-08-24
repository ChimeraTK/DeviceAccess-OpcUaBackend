/*
 * SubscriptionManager.h
 *
 *  Created on: Dec 17, 2020
 *      Author: Klaus Zenker (HZDR)
 */

#ifndef INCLUDE_SUBSCRIPTIONMANAGER_H_
#define INCLUDE_SUBSCRIPTIONMANAGER_H_

#include <map>
#include <deque>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <iostream>

#include "OPC-UA-Backend.h"
#include "OPC-UA-Connection.h"

#include <open62541/plugin/log_stdout.h>

namespace ChimeraTK{
  class OpcUABackendRegisterAccessorBase;

  /**
   * Struct used to store all information about a backend subscription, which is a monitored item belonging to a OPC UA subscription in terms of OPC UA.
   */
  struct MonitorItem{
    UA_NodeId _node; ///< Node id of the process variable to be monitored
    std::vector<OpcUABackendRegisterAccessorBase*> _accessors; ///< Pointer to the accessors using this item
    UA_UInt32 _id{0}; ///< ID of the monitored item that belongs to the subscription
    bool _active{false}; ///< If active the data is updated by the callback function
    bool _isMonitored{false}; ///< If true it is already added to the subscription as monitored item
    bool _hasException{false}; ///<True if exception is thrown by a certain item and used to avoid sending exception twice in deactivateAllAndPushException
    std::string _browseName; ///< browseName - used to compare monitored items -> \ToDo: use NodeStore?!

    MonitorItem(const std::string &browseName, const UA_NodeId& node, OpcUABackendRegisterAccessorBase* accessor): _node(node), _browseName(browseName){_accessors.push_back(accessor);};
    friend bool operator==(const MonitorItem& lhs, const MonitorItem &rhs){return lhs._browseName == rhs._browseName;}
    bool operator==(const std::string& other){return _browseName == other;}
  };

  /**
   * Class handling the OPC UA subscriptions and monitored items.
   *
   * So far only one subscription is used and all ctk subscriptions of BackendAccessors are monitored items to this subscription.
   */
  class OPCUASubscriptionManager{
  public:

    OPCUASubscriptionManager(std::shared_ptr<OPCUAConnection> connection);
    ~OPCUASubscriptionManager();


    static void
    deleteSubscriptionCallback(UA_Client *client, UA_UInt32 subscriptionId, void *subscriptionContext) {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Subscription Id %u was deleted", subscriptionId);
    }
    /**
     * Enable pushing values to the TransferElement future queue in the OPC UA callback function.
     *
     * \remark Holds item lock.
     */
    void activate();

    /**
     * Disable pushing values to the TransferElement future queue in the OPC UA callback function.
     * \ToDo: Should the following actions really be part of that method?
     * Unsubscribe all PVs from the OPC UA subscription.
     * Stop the runClient loop in case it was acitve.
     * To work again a resetClient is required.
     *
     * \remark This method is called when holding the client lock
     */
    void deactivate();

    /**
     * Push exception to the TransferElement future queue and call deactivate(True) keeping the _item list.
     * This is used in the responseHandler and when calling setException.
     *
     * \remark This method is called when holding the client lock
     */
    void deactivateAllAndPushException(std::string message = "Exception reported by another accessor.");

    /**
     * Remove a the current subscription.
     */
    void removeSubscription();

//    template<typename UAType, typename CTKType>
//    void subscribe(const UA_NodeId& id, OpcUABackendRegisterAccessor<UAType, CTKType>* accessor);

    /**
     * Store all accessors that are supposed to be used with the OPC UA subscription.
     * In case the subscription is already created (device was opened) monitored items are added to the subscription.
     * In case asyncRead was already activated in the Device also activate is called and pushing to the TransferElement
     * future queue is enabled.
     *
     *  \remark Holds item lock.
     */
    void subscribe(const std::string &browseName, const UA_NodeId& node, OpcUABackendRegisterAccessorBase* accessor);

    void unsubscribe(const std::string &browseName, OpcUABackendRegisterAccessorBase* accessor);

    /**
     * Start the thread that updates data of accessors.
     */
    void start();

    /**
     * Listen on the network and update accessors via the responseHandler. It is launched in a separate thread in start().
     *
     * \remark This method is called when holding the client lock
     */
    void runClient();

    void prepare();

    /// callback function for the client
//    template <typename UAType>
    static void responseHandler(UA_Client *client, UA_UInt32 subId, void *subContext,
        UA_UInt32 monId, void *monContext, UA_DataValue *value);

    bool isRunning(){return _run;}

    // Report an exception to the subscription manager. E.g. thrown by the RegisterAccessor.
    void setExternalError(const std::string &browseName);

    std::mutex _mutex; ///< Mutex used to protect writing non atomic member variables of items in the _items vector (can not use atomic because we put it into a vector of unknown size)

    /*
     *  To keep asynchronous services alive (e.g. renew secure channel,...) the client needs
     *  to call UA_run_iterate all the time, which is done in this thread.
     */
    std::unique_ptr<std::thread> _opcuaThread{nullptr};

    /*
     *  This is necessary if the client connection is reset.
     *  It will clear the subscription map and reset the MonitorItem status,
     *  such that they will be added as monitored items again when calling addMonitoredItems.
     *
     *  \remark Holds item lock.
     */

    void resetMonitoredItems();

    /**
     * Clear list of monitored items.
     *
     * Use this to make sure the corresponding items are not activated any more. This is important if the accessors do not
     * exist anymore when calling activate again.
     *
     *  \remark Holds item lock.
     */
    void removeMonitoredItems();
  private:

    /**
     * Here the items are registered to the server by the client.
     * This is to be called after the client is set up.
     * Called in activateAsyncRead()
     *
     * \remark It holds the client lock and item lock.
     */
    void addMonitoredItems();

    /**
     * Set up the subscription.
     * It is called by setClient().
     *
     * \remark This method holds the client lock.
     */
    void createSubscription();

    std::atomic<bool> _run{false};
    std::atomic<bool> _subscriptionActive{false};
    bool _asyncReadActive{false};

    std::shared_ptr<OPCUAConnection> _connection;

    UA_UInt32 _subscriptionID;

    // List of items to be monitored
    std::deque<MonitorItem> _items;

    /*
     *  map that links a monitoredItemId to the corresponding MonitoredItem in _items.
     *  This is needed because when adding MonitorItems to _items the monitoredItemIds are not known.
     *  Only after activate is called the monitoredItemIds are known. A map is used to allow fast
     *  access in the responseHandler. Else one would have to search in _items for the item with the correct
     *  monitoredItemId.
     */
    std::map<UA_UInt32, MonitorItem*> subscriptionMap;

    /*
     *  Send exception to all accesors via the future queue.
     * \remark Holds item lock.
     */
    void handleException(const std::string &msg);

  };
}



#endif /* INCLUDE_SUBSCRIPTIONMANAGER_H_ */
