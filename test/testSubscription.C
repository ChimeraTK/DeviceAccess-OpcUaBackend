/*
 * testSubscription.C
 *
 *  Created on: Apr 25, 2019
 *      Author: zenker
 */

#include "open62541.h"
#include <stdio.h>
#include <unistd.h>
#include <iostream>

bool changed;

static void
handler_TheAnswerChanged(UA_UInt32 monId, UA_DataValue *value, void *context) {
  UA_UInt32* tmp = (UA_UInt32*)value->value.data;
  printf("The Answer has changed: %d\n", *tmp);
  changed = true;
}

static void
handler_TimeChanged(UA_UInt32 monId, UA_DataValue *value, void *context) {
  UA_UInt32* tmp = (UA_UInt32*)value->value.data;
  printf("The time has changed: %d\n", *tmp);
}

int main(){
  UA_Client *client = UA_Client_new(UA_ClientConfig_standard);

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

  /* Create a subscription */
  UA_UInt32 subId = 0;
  UA_Client_Subscriptions_new(client, UA_SubscriptionSettings_standard, &subId);
  if(subId)
      printf("Create subscription succeeded, id %u\n", subId);
  /* Add a MonitoredItem */
//  UA_NodeId monitorThis = UA_NODEID_STRING(1, "/system/status/uptimeSec");
  UA_NodeId monitorThis = UA_NODEID_STRING(1, "watchdog_server/processes/0/config/killSigValue");
  UA_UInt32 monId = 0;
  UA_Client_Subscriptions_addMonitoredItem(client, subId, monitorThis, UA_ATTRIBUTEID_VALUE,
                                           &handler_TheAnswerChanged, NULL, &monId);
  if (monId)
      printf("Monitoring '/processes/0/config/killSig', id %u\n", subId);
  UA_NodeId monitorThis1 = UA_NODEID_STRING(1, "watchdog_server/system/status/uptimeSecValue");
  UA_Client_Subscriptions_addMonitoredItem(client, subId, monitorThis1, UA_ATTRIBUTEID_VALUE,
                                           &handler_TimeChanged, NULL, &monId);
  if (monId)
      printf("Monitoring 'the.answer', id %u\n", subId);
  /* The first publish request should return the initial value of the variable */
  UA_Client_Subscriptions_manuallySendPublishRequest(client);

  changed = false;
  while(!changed){
    UA_Client_Subscriptions_manuallySendPublishRequest(client);
  }
  UA_Client_disconnect(client);
  UA_Client_delete(client);
}

