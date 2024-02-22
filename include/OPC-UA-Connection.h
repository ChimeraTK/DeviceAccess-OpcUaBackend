// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
/*
 * OPC-UA-Connection.h
 *
 *  Created on: Jan 29, 2021
 *      Author: Klaus Zenker (HZDR)
 */
#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <open62541/plugin/log_stdout.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

namespace ChimeraTK {

  struct UAClientDeleter {
    void operator()(UA_Client* client) { UA_Client_delete(client); }
  };

  struct OPCUAConnection {
    // This needs to be public because it is accessed by the RegisterAccessor.
    // Can not be a shared pointer because the struct is only defined in the source file...

    std::unique_ptr<UA_Client, UAClientDeleter> client;
    UA_ClientConfig* config;
    std::string serverAddress;

    std::atomic<UA_SecureChannelState> channelState;
    std::atomic<UA_SessionState> sessionState;

    /**
     * This is used since async access is not supported by OPC-UA.
     [12/26/2018 20:55:49.705] error/client Reply answers the wrong requestId. Async services are not yet implemented.
     [12/26/2018 20:55:49.705] info/client  Error receiving the response
     * One could also use a client per process variable...
     */
    std::mutex client_lock;

    std::string username;
    std::string password;

    unsigned long publishingInterval;
    unsigned long connectionTimeout;

    UA_Logger logger;

    OPCUAConnection(const std::string& address, const std::string& username, const std::string& password,
        unsigned long publishingInterval, const long int& connectionTimeout, const UA_LogLevel& logLevel)
    : client(UA_Client_new()), config(UA_Client_getConfig(client.get())), serverAddress(address),
      channelState(UA_SECURECHANNELSTATE_CLOSED), sessionState(UA_SESSIONSTATE_CLOSED), username(username),
      password(password), publishingInterval(publishingInterval), connectionTimeout(connectionTimeout),
      logger(UA_Log_Stdout_withLevel(logLevel)) {
      UA_ClientConfig_setDefault(config);
      config->timeout = connectionTimeout;
    };

    void close() {
      for(size_t i = 0; i < 2; ++i) {
        /*
         * This loop is introduced to fix test B_9_1 of the UnifiedBackendTest.
         * Without, client->connection.state == UA_CONNECTIONSTATE_OPENING after unlocking
         * the server in the tests in the write part of B_9_1. In that state calling UA_Client_connect fails because
         * the connection is not fully setup in connectIterate(...) (see ua_client_connect.c)
         * Calling it once, however sets client->connection.state == UA_CONNECTIONSTATE_OPENING
         * in the synchronous part of B_9_1. That is why its called twice here to force
         * client->connection.state == UA_CONNECTIONSTATE_CLOSED, which forces calling
         * initConnect(client) in connectIterate(...).
         * \ToDo: Figure out how to do it correct!
         */
        UA_Client_run_iterate(client.get(), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(connectionTimeout));
      }
      auto ret = UA_Client_disconnect(client.get());
      if(ret != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(config->logging, UA_LOGCATEGORY_USERLAND,
            "Failed to disconnect from server when closing the device. Error: %s", UA_StatusCode_name(ret));
      }
    }

    // Check connection state set by the callback function.
    bool isConnected() const {
      return (sessionState == UA_SESSIONSTATE_ACTIVATED && channelState == UA_SECURECHANNELSTATE_OPEN);
    }
  };

} // namespace ChimeraTK
