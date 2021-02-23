/*
 * testSubscription.C
 *
 *  Created on: Apr 25, 2019
 *      Author: zenker
 */

#include <open62541/client_highlevel.h>
#include <open62541/client_config_default.h>
#include <open62541/client_subscriptions.h>
#include <open62541/plugin/log_stdout.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>

bool changed;

static void
handler_TheAnswerChanged(UA_Client *client, UA_UInt32 subId, void *subContext,
    UA_UInt32 monId, void *monContext, UA_DataValue *value) {
  UA_UInt32* tmp = (UA_UInt32*)value->value.data;
  printf("The Answer has changed: %d\n", *tmp);
  changed = true;
}

static void
handler_TimeChanged(UA_Client *client, UA_UInt32 subId, void *subContext,
    UA_UInt32 monId, void *monContext, UA_DataValue *value) {
  UA_UInt32* tmp = (UA_UInt32*)value->value.data;
  printf("The time has changed: %d\n", *tmp);
}

static void
deleteSubscriptionCallback(UA_Client *client, UA_UInt32 subscriptionId, void *subscriptionContext) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Subscription Id %u was deleted", subscriptionId);
}

static void
subscriptionInactivityCallback (UA_Client *client, UA_UInt32 subId, void *subContext) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Inactivity for subscription %u", subId);
}

static void
stateCallback(UA_Client *client, UA_SecureChannelState channelState,
              UA_SessionState sessionState, UA_StatusCode recoveryStatus) {
    switch(channelState) {
    case UA_SECURECHANNELSTATE_CLOSED:
      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "The client is disconnected");
      break;
    case UA_SECURECHANNELSTATE_HEL_SENT:
      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Waiting for ack");
      break;
    case UA_SECURECHANNELSTATE_OPN_SENT:
      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Waiting for OPN Response");
      break;
    case UA_SECURECHANNELSTATE_OPEN:
      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "A SecureChannel to the server is open");
      break;
    default:
      break;
    }

    switch(sessionState) {
    case UA_SESSIONSTATE_ACTIVATED: {
        /* Create a subscription */
        UA_UInt32 subId = 0;
        UA_CreateSubscriptionRequest request = UA_CreateSubscriptionRequest_default();
        UA_CreateSubscriptionResponse response =
                    UA_Client_Subscriptions_create(client, request, NULL, NULL, deleteSubscriptionCallback);
        if(response.responseHeader.serviceResult == UA_STATUSCODE_GOOD){
          subId = response.subscriptionId;
            printf("Create subscription succeeded, id %u\n", subId);
        }
        /* Add a MonitoredItem */
      //  UA_NodeId monitorThis = UA_NODEID_STRING(1, "/system/status/uptimeSec");
        UA_NodeId monitorThis = UA_NODEID_STRING(1, "watchdog_server/processes/0/config/killSigValue");
        UA_UInt32 monId = 0;
        UA_MonitoredItemCreateRequest monRequest =
            UA_MonitoredItemCreateRequest_default(monitorThis);

        UA_MonitoredItemCreateResult monResponse =
            UA_Client_MonitoredItems_createDataChange(client, response.subscriptionId,
                                                      UA_TIMESTAMPSTORETURN_BOTH, monRequest,
                                                      NULL, handler_TheAnswerChanged, NULL);

        if (monResponse.statusCode == UA_STATUSCODE_GOOD){
          monId = monResponse.monitoredItemId;
          printf("Monitoring '/processes/0/config/killSig', id %u\n", subId);
        }
        UA_NodeId monitorThis1 = UA_NODEID_STRING(1, "watchdog_server/system/status/uptimeSecValue");
        monRequest =
              UA_MonitoredItemCreateRequest_default(monitorThis1);

        monResponse =
              UA_Client_MonitoredItems_createDataChange(client, response.subscriptionId,
                                                        UA_TIMESTAMPSTORETURN_BOTH, monRequest,
                                                        NULL, handler_TheAnswerChanged, NULL);

        if (monResponse.statusCode == UA_STATUSCODE_GOOD){
          monId = monResponse.monitoredItemId;
          printf("Monitoring 'the.answer', id %u\n", monId);
        }
      }
      break;
    case UA_SESSIONSTATE_CLOSED:
      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Session disconnected");
      break;
    default:
      break;
    }
}

int main(){
  UA_Client *client = UA_Client_new();
  UA_ClientConfig *cc = UA_Client_getConfig(client);
  cc->stateCallback = stateCallback;
  cc->subscriptionInactivityCallback = subscriptionInactivityCallback;
  /* Connect to a server */
  /* anonymous connect would be: retval = UA_Client_connect(client, "opc.tcp://localhost:16664"); */
  UA_StatusCode retval = UA_Client_connect(client, "opc.tcp://localhost:11000");
  if(retval != UA_STATUSCODE_GOOD) {
      UA_Client_delete(client);
      return (int)retval;
  }

  /* Simple read */
  UA_Variant* var = UA_Variant_new();
  retval = UA_Client_readValueAttribute(client, UA_NODEID_STRING(1, "watchdog_server/system/status/uptimeSecValue"), var);

  if(retval != UA_STATUSCODE_GOOD){
    std::cout << "Failed reading simple." << std::endl;
  } else {
    UA_UInt32* tmp = (UA_UInt32*)var->data;
    std::cout << "Data is: "<< tmp[0] << std::endl;
  }

  changed = false;
  while(!changed){
    UA_Client_run_iterate(client, 1000);
  }
  UA_Client_disconnect(client);
  UA_Client_delete(client);
}

