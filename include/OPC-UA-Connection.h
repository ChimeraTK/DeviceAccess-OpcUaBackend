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

#include "open62541.h"

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
  UA_ClientConfig config;
  std::string serverAddress;

  /**
   * This is used since async access is not supported by OPC-UA.
   [12/26/2018 20:55:49.705] error/client Reply answers the wrong requestId. Async services are not yet implemented.
   [12/26/2018 20:55:49.705] info/client  Error receiving the response
   * One could also use a client per process variable...
   */

  std::mutex lock;

  std::string username;
  std::string password;

  unsigned long port;
  unsigned long publishingInterval;
};

}

#endif /* INCLUDE_OPC_UA_CONNECTION_H_ */
