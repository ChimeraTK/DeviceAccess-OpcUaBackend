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
#include <open62541/plugin/securitypolicy.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <string>
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
    std::string certificate;
    std::string key;

    unsigned long publishingInterval;
    unsigned long connectionTimeout;

    UA_Logger logger;

    OPCUAConnection(const std::string& address, const std::string& username, const std::string& password,
        unsigned long publishingInterval, const long int& connectionTimeout, const UA_LogLevel& logLevel,
        const std::string& certificate, const std::string& privateKey)
    : client(UA_Client_new()), config(UA_Client_getConfig(client.get())), serverAddress(address),
      channelState(UA_SECURECHANNELSTATE_CLOSED), sessionState(UA_SESSIONSTATE_CLOSED), username(username),
      password(password), certificate(certificate), key(privateKey), publishingInterval(publishingInterval),
      connectionTimeout(connectionTimeout), logger(UA_Log_Stdout_withLevel(logLevel)) {
      config->logging = &logger;
      config->eventLoop->logger = &logger;
      if(!certificate.empty() && !key.empty()) {
        config->securityMode = UA_MESSAGESECURITYMODE_SIGNANDENCRYPT;
        UA_ByteString privateKey = UA_BYTESTRING_NULL;
        privateKey = loadFile(key.c_str());
        UA_ByteString cert = UA_BYTESTRING_NULL;
        cert = loadFile(certificate.c_str());
        UA_ClientConfig_setDefaultEncryption(config, cert, privateKey, 0, 0, NULL, 0);
        if(!username.empty() && !password.empty()) {
          UA_UserNameIdentityToken* identityToken = UA_UserNameIdentityToken_new();
          identityToken->userName = UA_STRING_ALLOC(username.c_str());
          identityToken->password = UA_STRING_ALLOC(password.c_str());
          UA_ExtensionObject_clear(&config->userIdentityToken);
          config->userIdentityToken.encoding = UA_EXTENSIONOBJECT_DECODED;
          config->userIdentityToken.content.decoded.type = &UA_TYPES[UA_TYPES_USERNAMEIDENTITYTOKEN];
          config->userIdentityToken.content.decoded.data = identityToken;
        }
        UA_ByteString_clear(&cert);
        UA_ByteString_clear(&privateKey);
      }
      else {
        UA_ClientConfig_setDefault(config);
      }
      config->timeout = connectionTimeout;
    };

    void close() {
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

   private:
    /**
     * loadFile parses the certificate file.
     *
     * @param  path               specifies the file name given in argv[]
     * @return Returns the file content after parsing
     */
    static UA_INLINE UA_ByteString loadFile(const char* const path) {
      UA_ByteString fileContents = UA_STRING_NULL;

      /* Open the file */
      FILE* fp = fopen(path, "rb");
      if(!fp) {
        errno = 0; /* We read errno also from the tcp layer... */
        return fileContents;
      }

      /* Get the file length, allocate the data and read */
      fseek(fp, 0, SEEK_END);
      fileContents.length = (size_t)ftell(fp);
      fileContents.data = (UA_Byte*)UA_malloc(fileContents.length * sizeof(UA_Byte));
      if(fileContents.data) {
        fseek(fp, 0, SEEK_SET);
        size_t read = fread(fileContents.data, sizeof(UA_Byte), fileContents.length, fp);
        if(read != fileContents.length) UA_ByteString_clear(&fileContents);
      }
      else {
        fileContents.length = 0;
      }
      fclose(fp);

      return fileContents;
    }
  };

} // namespace ChimeraTK
