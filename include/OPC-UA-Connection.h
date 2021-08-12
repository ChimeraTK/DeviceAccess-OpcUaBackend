/*
 * OPC-UA-Connection.h
 *
 *  Created on: Jan 29, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#ifndef INCLUDE_OPC_UA_CONNECTION_H_
#define INCLUDE_OPC_UA_CONNECTION_H_

#include <memory>
#include <mutex>

#include <open62541/client_highlevel.h>
#include <open62541/client_config_default.h>
#include <open62541/plugin/log_stdout.h>

namespace ChimeraTK{

struct UAClientDeleter{
  void operator()(UA_Client* client){
    UA_Client_delete(client);
  }
};

struct OPCUAConnection{

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

  unsigned long port;
  unsigned long publishingInterval;

  OPCUAConnection(const std::string &address, const std::string &username, const std::string &password, unsigned long port, unsigned long publishingInterval):
    client(UA_Client_new()), config(UA_Client_getConfig(client.get())), serverAddress(address), channelState(UA_SECURECHANNELSTATE_FRESH),
    sessionState(UA_SESSIONSTATE_CLOSED), username(username), password(password), port(port), publishingInterval(publishingInterval)
  {UA_ClientConfig_setDefault(config);};

  void close(){
    if(!UA_Client_disconnect(client.get())){
      UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
        "Failed to disconnect from server when closing the device.");
    }
  }
};

}

#endif /* INCLUDE_OPC_UA_CONNECTION_H_ */
