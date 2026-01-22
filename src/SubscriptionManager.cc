// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * SubscriptionManager.cc
 *
 *  Created on: Dec 17, 2020
 *      Author: Klaus Zenker (HZDR)
 */

#include "SubscriptionManager.h"

#include "OPC-UA-BackendRegisterAccessor.h"

#include <ChimeraTK/Exception.h>

#include <open62541/client.h>
#include <open62541/client_subscriptions.h>
#include <open62541/plugin/log.h>
#include <sys/types.h>

#include <chrono>
#include <memory>

namespace ChimeraTK {

  void OPCUASubscriptionManager::deleteSubscriptionCallback(
      UA_Client* client, UA_UInt32 subscriptionId, void* /*subscriptionContext*/) {
    UA_LOG_INFO(
        &OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND, "Subscription Id %u was deleted", subscriptionId);
    if(OpcUABackend::backendClients.count(client) == 0) {
      UA_LOG_WARNING(
          &OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND, "No client found in the deleteSubscriptionCallback.");
      return;
    }
    OpcUABackend::backendClients[client]->_subscriptionManager->setInactive();
  };

  OPCUASubscriptionManager::~OPCUASubscriptionManager() {
    // already called when closing the device...
    deactivate();
    if(opcuaThread && opcuaThread->joinable()) {
      opcuaThread->join();
      opcuaThread.reset(nullptr);
    }
  }

  void OPCUASubscriptionManager::start() {
    //\ToDo: In fact this thread will always run since the subscription is always activated by the backend (Needed for
    // adding subscription after device open and activateAsyncRead).
    if(_subscriptionActive) {
      _run = true;
      UA_LOG_INFO(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
          "Starting subscription thread with publishing interval of %fms.", _connection->publishingInterval);
      opcuaThread = std::make_unique<std::thread>(&OPCUASubscriptionManager::runClient, this);
    }
    else {
      _run = false;
      UA_LOG_INFO(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
          "No active subscriptions. No need to start the subscription manager.");
    }
  }

  void OPCUASubscriptionManager::runClient() {
    UA_StatusCode ret;
    UA_LOG_DEBUG(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND, "Starting client iterate loop.");
    uint64_t i = 0;
    while(_run) {
      UA_LOG_TRACE(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND, "Sending subscription request.");
      {
        std::lock_guard<std::mutex> lock(_connection->client_lock);
        ret = UA_Client_run_iterate(_connection->client.get(), 0);
        if(_subscriptionNeedsToBeRemoved) {
          break;
        }
      }
      if(ret != UA_STATUSCODE_GOOD) {
        UA_LOG_INFO(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
            "Stopped sending publish requests. OPC UA message: %s", UA_StatusCode_name(ret));

        break;
      }
      // sleep for half the publishing interval - should be fine to catch up all updates
      std::this_thread::sleep_for(std::chrono::milliseconds((uint32_t)_connection->publishingInterval / 2));
      ++i;
      if(i % 50 == 0) {
        UA_LOG_DEBUG(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND, "Still running client iterate loop.");
      }
    }
    {
      std::lock_guard<std::mutex> lock(_connection->client_lock);
      if(_subscriptionNeedsToBeRemoved) {
        removeSubscription();
        _subscriptionNeedsToBeRemoved = false;
      }
    }
    UA_LOG_INFO(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND, "Stopped client iterate loop.");

    // Inform all accessors that are subscribed
    //  if(_run)
    //    handleException("OPC UA connection lost.");
    _run = false;
  }

  void OPCUASubscriptionManager::activate() {
    if(!_subscriptionActive) {
      createSubscription();
    }
    _asyncReadActive = true;
    if(_items.size() > 0 && subscriptionMap.size() == 0) {
      addMonitoredItems();
    }
    else {
      std::lock_guard<std::mutex> lock(mutex);
      for(auto& item : _items) {
        item.active = true;
      }
    }
  }

  void OPCUASubscriptionManager::deactivate() {
    // check if subscription thread was started at all. If yes _run was set true in OPCUASubscriptionManager::start()
    //\ToDo: can we use resetMonitoredItems here?
    {
      std::lock_guard<std::mutex> lock(mutex);
      for(auto& item : subscriptionMap) {
        item.second->active = false;
      }
    }
    if(_run) {
      _run = false;
    }

    if(_subscriptionActive) {
      _subscriptionActive = false;

      if(opcuaThread) {
        // lock is hold in runClient and this method is called in the state callback by the same thread in in
        // UA_Client_run_iterate() -> simply set bool and remove subscription outside UA_Client_run_iterate()
        // in the main thread!
        _subscriptionNeedsToBeRemoved = true;
      }
      else {
        std::lock_guard<std::mutex> lock(_connection->client_lock);
        removeSubscription();
      }
    }
    _asyncReadActive = false;
  }

  void OPCUASubscriptionManager::removeSubscription() {
    if(!UA_Client_Subscriptions_deleteSingle(_connection->client.get(), _subscriptionID)) {
      UA_LOG_DEBUG(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND, "Successfully removed old subscription.");
    }
    // reset ID even subscription deletion fails - this avoids removing it later again
    _subscriptionID = 0;
    resetMonitoredItems();
  }

  void OPCUASubscriptionManager::deactivateAllAndPushException(const std::string& message) {
    handleException(message);
    deactivate();
  }

  void OPCUASubscriptionManager::responseHandler(UA_Client* /*client*/, UA_UInt32 subId, void* /*subContext*/,
      UA_UInt32 monId, void* monContext, UA_DataValue* value) {
    UA_DateTime sourceTime = value->sourceTimestamp;
    UA_DateTimeStruct dts = UA_DateTime_toStruct(sourceTime + UA_DateTime_localTimeUtcOffset());
    UA_LOG_DEBUG(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
        "Subscription handler called. Source time stamp: %02u-%02u-%04u %02u:%02u:%02u.%03u", dts.day, dts.month,
        dts.year, dts.hour, dts.min, dts.sec, dts.milliSec);
    auto* base = reinterpret_cast<OPCUASubscriptionManager*>(monContext);

    try {
      if(base->subscriptionMap.at(monId)->active) {
        // only lock the mutex if active. This is used when unsubscribing to avoid dead locks
        std::lock_guard<std::mutex> lock(base->mutex);
        base->subscriptionMap[monId]->hasException = false;
        UA_LOG_DEBUG(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND, "Pushing data to queue for %zu accessors.",
            base->subscriptionMap[monId]->accessors.size());
        for(auto& accessor : base->subscriptionMap[monId]->accessors) {
          UA_DataValue data;
          UA_DataValue_init(&data);
          UA_DataValue_copy(value, &data);
          accessor->notifications.push_overwrite(std::move(data));
        }
      }
    }
    catch(std::out_of_range& e) {
      // When calling unsubscribe the item is removed before it is unsubscribed from the client, which might trigger the
      // response handler.
      UA_LOG_ERROR(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
          "Response handler for monitored item with id %d called but item is already removed.", subId);
    }
  }

  void OPCUASubscriptionManager::createSubscription() {
    /* Clean up left over subscription. */
    if(_subscriptionID != 0) {
      removeSubscription();
    }
    /* Create the subscription with default configuration. */
    UA_CreateSubscriptionRequest request = UA_CreateSubscriptionRequest_default();
    request.requestedPublishingInterval = _connection->publishingInterval;
    UA_CreateSubscriptionResponse response;
    {
      std::lock_guard<std::mutex> lock(_connection->client_lock);
      response =
          UA_Client_Subscriptions_create(_connection->client.get(), request, NULL, NULL, deleteSubscriptionCallback);
    }
    if(response.responseHeader.serviceResult == UA_STATUSCODE_GOOD) {
      _subscriptionID = response.subscriptionId;
      UA_LOG_DEBUG(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND, "Create subscription succeeded, id %u",
          _subscriptionID);
      if(response.revisedPublishingInterval != _connection->publishingInterval) {
        UA_LOG_WARNING(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
            "Publishing interval was changed from %fms to %fms", _connection->publishingInterval,
            response.revisedPublishingInterval);
        // replace publishing interval with revised publishing interval as it will be used to set the sampling interval
        // of monitored items
        _connection->publishingInterval = response.revisedPublishingInterval;
      }
      _subscriptionActive = true;
    }
    else {
      throw ChimeraTK::runtime_error("Failed to set up subscription.");
    }
  }

  void OPCUASubscriptionManager::addMonitoredItems() {
    mutex.lock();
    for(auto& item : _items) {
      if(_asyncReadActive) {
        item.active = true;
      }
      if(!item.isMonitored) {
        // create monitored item
        // pass object as context to the callback function. This allows to use individual subscriptionMaps for each manager!
        UA_MonitoredItemCreateRequest monRequest = UA_MonitoredItemCreateRequest_default(item.node);
        // sampling interval equal to the publishing interval set for the subscription
        if(!item.accessors.at(0)->info->indexRange.empty()) {
          UA_LOG_INFO(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
              "Using data range %s for monitored item of %s.", item.accessors.at(0)->info->indexRange.c_str(),
              item.browseName.c_str());
          monRequest.itemToMonitor.indexRange = UA_String_fromChars(item.accessors.at(0)->info->indexRange.c_str());
        }

        monRequest.requestedParameters.samplingInterval = _connection->publishingInterval;
        UA_MonitoredItemCreateResult monResponse;
        // unlock mutex because the OPC UA call potentially ends up in a state callback which enters deactivateAllAndPushException
        mutex.unlock();
        {
          std::lock_guard<std::mutex> lock(_connection->client_lock);
          monResponse = UA_Client_MonitoredItems_createDataChange(_connection->client.get(), _subscriptionID,
              UA_TIMESTAMPSTORETURN_BOTH, monRequest, this, &OPCUASubscriptionManager::responseHandler, NULL);
        }
        mutex.lock();
        /* Check server response to adding the item to be monitored. */
        if(monResponse.statusCode == UA_STATUSCODE_GOOD) {
          item.id = monResponse.monitoredItemId;
          UA_LOG_DEBUG(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND, "Monitoring id %u (%s) for pv: %s",
              item.id, _connection->serverAddress.c_str(), item.browseName.c_str());
          if(monResponse.revisedSamplingInterval != _connection->publishingInterval) {
            UA_LOG_WARNING(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
                "Publishing interval was changed from %fms to %fms", _connection->publishingInterval,
                monResponse.revisedSamplingInterval);
          }
          subscriptionMap[item.id] = &item;
          item.isMonitored = true;
        }
        else {
          mutex.unlock();
          handleException(std::string("Failed to add monitored item for node: ") + item.browseName +
              " Error: " + UA_StatusCode_name(monResponse.statusCode));
          mutex.lock();
        }
      }
    }
    mutex.unlock();
  }

  void OPCUASubscriptionManager::prepare() {
    createSubscription();
    addMonitoredItems();
  }

  void OPCUASubscriptionManager::subscribe(
      const std::string& browseName, const UA_NodeId& node, OpcUABackendRegisterAccessorBase* accessor) {
    //\ToDo: Check if already monitored based on node using the NodeStore?!
    std::deque<MonitorItem>::iterator it;
    mutex.lock();
    it = std::find(_items.begin(), _items.end(), browseName);

    if(it == _items.end()) {
      /* Request monitoring for the node of interest. */
      MonitorItem item(browseName, node, accessor);
      _items.push_back(item);

      mutex.unlock();
      // check if device was already opened
      if(_asyncReadActive) {
        // This can happen if async read was activated without any RegisterAccessors using the subscription.
        // In this case it is closed due to inactivity.
        if(!_subscriptionActive) {
          UA_LOG_WARNING(
              &OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND, "No active subscription. Setting up new one.");
          createSubscription();
        }
        addMonitoredItems();
      }
    }
    else {
      UA_LOG_DEBUG(
          &OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND, "Adding accessor to existing node subscription.");
      auto* tmp = it->accessors.back();
      it->accessors.push_back(accessor);
      if(it->active) {
        // if already active add initial value
        UA_LOG_DEBUG(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
            "Setting initial value for accessor with existing node subscription.");
        std::lock_guard<std::mutex> lock(tmp->dataUpdateLock);
        // wait for initial values that might be process in postRead at the moment
        if(tmp->notifications.empty()) {
          if(!UA_Variant_isEmpty(&tmp->data.value)) {
            UA_DataValue data;
            UA_DataValue_init(&data);
            UA_DataValue_copy(&tmp->data, &data);
            accessor->notifications.push_overwrite(std::move(data));
          }
          else {
            UA_LOG_DEBUG(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
                "No initial value available for accessor with existing node subscription.");
          }
        }
        else {
          auto dataFromQueue = tmp->notifications.front();
          if(!UA_Variant_isEmpty(&dataFromQueue.value)) {
            UA_DataValue data;
            UA_DataValue_init(&data);
            UA_DataValue_copy(&tmp->data, &data);
            accessor->notifications.push_overwrite(std::move(data));
          }
        }
      }
      mutex.unlock();
    }
  }

  OPCUASubscriptionManager::OPCUASubscriptionManager(std::shared_ptr<OPCUAConnection> connection)
  : _connection(connection) {}

  void OPCUASubscriptionManager::resetMonitoredItems() {
    std::lock_guard<std::mutex> lock(mutex);
    for(auto& item : _items) {
      item.isMonitored = false;
    }
    subscriptionMap.clear();
  }

  void OPCUASubscriptionManager::stopClientThread() {
    if(opcuaThread && opcuaThread->joinable()) {
      opcuaThread->join();
      opcuaThread.reset(nullptr);
    }
  }
  void OPCUASubscriptionManager::unsubscribe(
      const std::string& browseName, OpcUABackendRegisterAccessorBase* accessor) {
    // If the id is set an item is to be removed from the client. Before the _mutex lock is released.
    UA_UInt32 id{0};
    {
      std::lock_guard<std::mutex> item_lock(mutex);
      // client pointer might be reset already when closing the device - in this case nothing to do here
      auto it = std::find(_items.begin(), _items.end(), browseName);
      // in case the asyncread was activated but no variables were subscribed - unsubscribe is called by RegisterAccessor destructor
      if(it == _items.end()) {
        return;
      }
      if(it->accessors.size() > 1) {
        // only remove accessor if still other accessors are using that subscription
        it->accessors.erase(std::find(it->accessors.begin(), it->accessors.end(), accessor));
      }
      else {
        // remove monitored item
        subscriptionMap.erase(it->id);
        id = it->id;
        _items.erase(it);
      }
    }
    // try to unsubscribe
    if(id != 0 && _connection->isConnected()) {
      UA_StatusCode ret;
      {
        std::lock_guard<std::mutex> connection_lock(_connection->client_lock);
        // UA_Client_MonitoredItems_deleteSingle tries to update the latest value which triggers responseHandler
        ret = UA_Client_MonitoredItems_deleteSingle(_connection->client.get(), _subscriptionID, id);
      }

      if(!ret) {
        UA_LOG_DEBUG(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND, "Monitored item removed for: %s",
            browseName.c_str());
      }
      else {
        UA_LOG_ERROR(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
            "Failed to unsubscribe item (no client alive): %s Error: %s", browseName.c_str(), UA_StatusCode_name(ret));
      }
      if(_items.size() == 0) {
        // remove subscription
        {
          std::lock_guard<std::mutex> connection_lock(_connection->client_lock);
          removeSubscription();
        }
        _run = false;
        stopClientThread();
      }
    }
  }

  void OPCUASubscriptionManager::handleException(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex);
    UA_LOG_INFO(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND, "Handling error: %s", message.c_str());
    for(auto& item : _items) {
      try {
        throw ChimeraTK::runtime_error(message);
      }
      catch(...) {
        if(item.active && !item.hasException) {
          item.hasException = true;
          UA_LOG_INFO(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
              "Sending exception to %lu accessors for node %s.", item.accessors.size(), item.browseName.c_str());
          for(auto& accessor : item.accessors) {
            accessor->notifications.push_overwrite_exception(std::current_exception());
          }
        }
      }
    }
  }

  void OPCUASubscriptionManager::setExternalError(const std::string& browseName) {
    auto it = std::find(_items.begin(), _items.end(), browseName);
    if(it != _items.end()) {
      std::lock_guard<std::mutex> lock(mutex);
      it->hasException = true;
    }
  }

} // namespace ChimeraTK
