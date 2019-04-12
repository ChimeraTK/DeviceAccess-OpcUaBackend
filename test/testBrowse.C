#ifdef UA_NO_AMALGAMATION
# include "ua_types.h"
# include "ua_client.h"
# include "ua_client_highlevel.h"
# include "ua_nodeids.h"
# include "ua_network_tcp.h"
# include "ua_config_standard.h"
#else
# include "open62541.h"
# include <string.h>
# include <stdlib.h>
#include <string>
#include <set>
#include <iterator>
#endif

#include <stdio.h>
#include "OPC-UA-Backend.h"


static void
handler_TheAnswerChanged(UA_UInt32 monId, UA_DataValue *value, void *context) {
    printf("The Answer has changed!\n");
}

static UA_StatusCode
nodeIter(UA_NodeId childId, UA_Boolean isInverse, UA_NodeId referenceTypeId, void *handle) {
    if(isInverse)
        return UA_STATUSCODE_GOOD;
    UA_NodeId *parent = (UA_NodeId *)handle;
    printf("%d, %d --- %d ---> NodeId %d, %d\n",
           parent->namespaceIndex, parent->identifier.numeric,
           referenceTypeId.identifier.numeric, childId.namespaceIndex,
           childId.identifier.numeric);
    return UA_STATUSCODE_GOOD;
}

void readValue(UA_String node, UA_Client *_client){
    UA_Int32 value = 0;
    //std::cout << "Reading the value of node (1, \"" << _node_id << "\":" << std::endl;
    UA_Variant *val = UA_Variant_new();
    UA_StatusCode retval = UA_Client_readValueAttribute(_client, UA_NODEID_STRING(1, (char*)(node.data)), val);
    if(retval == UA_STATUSCODE_GOOD && UA_Variant_isScalar(val) &&
      val->type == &UA_TYPES[UA_TYPES_INT32]) {
      value = *(UA_Int32*)val->data;
      printf("the value is: %i\n", value);
    } else if (retval == UA_STATUSCODE_GOOD && UA_Variant_isScalar(val) &&
      val->type == &UA_TYPES[UA_TYPES_UINT32]){
      value = *(UA_UInt32*)val->data;
      printf("the value is: %i\n", value);
    } else if (retval == UA_STATUSCODE_GOOD && UA_Variant_isScalar(val) &&
      val->type == &UA_TYPES[UA_TYPES_STRING]){
      UA_String str = *(UA_String*)val->data;
      printf("the value is: %-16.*s\n", str.length, str.data);
    }
    UA_Variant_delete(val);
}

void getType(UA_String node, UA_Client *_client){
  UA_NodeId *outDataType = UA_NodeId_new();
  UA_Client_readDataTypeAttribute(_client, UA_NODEID_STRING(1, (char*)node.data), outDataType);

  if(UA_findDataType(outDataType) == &UA_TYPES[UA_TYPES_UINT32])
    printf("Filling catalogue with data type... %d (uint32)\n", outDataType->identifier.numeric);
  else if(UA_findDataType(outDataType) == &UA_TYPES[UA_TYPES_INT32])
    printf("Filling catalogue with data type... %d (int32)\n", outDataType->identifier.numeric);
  else if(UA_findDataType(outDataType) == &UA_TYPES[UA_TYPES_STRING])
    printf("Filling catalogue with data type... %d (string)\n", outDataType->identifier.numeric);
  else if(UA_findDataType(outDataType) == &UA_TYPES[UA_TYPES_DOUBLE])
    printf("Filling catalogue with data type... %d (double)\n", outDataType->identifier.numeric);
  else
    printf("Unknown type... %d\n", outDataType->identifier.numeric);
  UA_NodeId_delete(outDataType);

}

ChimeraTK::UASet browse(UA_Client *client, UA_NodeId node){
  ChimeraTK::UASet nodes;
  /* Browse some objects */
  printf("Browsing nodes in objects folder:\n");
  UA_BrowseRequest bReq;
  UA_BrowseRequest_init(&bReq);
  bReq.requestedMaxReferencesPerNode = 0;
  bReq.nodesToBrowse = UA_BrowseDescription_new();
  bReq.nodesToBrowseSize = 1;
  bReq.nodesToBrowse[0].nodeId = node; /* browse objects folder */
  bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL; /* return everything */
  UA_BrowseResponse bResp = UA_Client_Service_browse(client, bReq);
  printf("%-9s %-16s %-16s %-16s\n", "NAMESPACE", "NODEID", "BROWSE NAME", "DISPLAY NAME");
  for (size_t i = 0; i < bResp.resultsSize; ++i) {
      for (size_t j = 0; j < bResp.results[i].referencesSize; ++j) {
          UA_ReferenceDescription *ref = &(bResp.results[i].references[j]);
          if(ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_NUMERIC) {
              printf("%-9d %-16d %-16.*s %-16.*s\n", ref->nodeId.nodeId.namespaceIndex,
                     ref->nodeId.nodeId.identifier.numeric, (int)ref->browseName.name.length,
                     ref->browseName.name.data, (int)ref->displayName.text.length,
                     ref->displayName.text.data);
          } else if(ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_STRING) {
              printf("%-9d %-16.*s %-16.*s  \n", ref->nodeId.nodeId.namespaceIndex,
                     (int)ref->nodeId.nodeId.identifier.string.length,
                     ref->nodeId.nodeId.identifier.string.data,
                     (int)ref->browseName.name.length, ref->browseName.name.data,
                     (int)ref->displayName.text.length, ref->displayName.text.data);
          }
          if(ref->nodeId.nodeId.namespaceIndex == 1){
            if(ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_NUMERIC) {
              nodes.insert(ref->nodeId.nodeId);
            }
            readValue(ref->displayName.text,client);
            getType(ref->displayName.text,client);
          }
          /* TODO: distinguish further types */
      }
  }
  UA_BrowseRequest_deleteMembers(&bReq);
  UA_BrowseResponse_deleteMembers(&bResp);
  return nodes;
}

int main(int argc, char *argv[]) {
    UA_Client *client = UA_Client_new(UA_ClientConfig_standard);

    /* Listing endpoints */
    UA_EndpointDescription* endpointArray = NULL;
    size_t endpointArraySize = 0;
    UA_StatusCode retval = UA_Client_getEndpoints(client, "opc.tcp://localhost:11000",
                                                  &endpointArraySize, &endpointArray);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_Array_delete(endpointArray, endpointArraySize, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
        UA_Client_delete(client);
        return (int)retval;
    }
    printf("%i endpoints found\n", (int)endpointArraySize);
    for(size_t i=0;i<endpointArraySize;i++){
        printf("URL of endpoint %i is %.*s\n", (int)i,
               (int)endpointArray[i].endpointUrl.length,
               endpointArray[i].endpointUrl.data);
    }
    UA_Array_delete(endpointArray,endpointArraySize, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);

    /* Connect to a server */
    /* anonymous connect would be: retval = UA_Client_connect(client, "opc.tcp://localhost:16664"); */
    retval = UA_Client_connect(client, "opc.tcp://localhost:11000");
    if(retval != UA_STATUSCODE_GOOD) {
        UA_Client_delete(client);
        return (int)retval;
    }

    ChimeraTK::UASet l1 = browse(client, UA_NODEID_NUMERIC(0,UA_NS0ID_OBJECTSFOLDER));
    for(ChimeraTK::UASet::iterator it = l1.begin(); it != l1.end();it++){
      ChimeraTK::UASet l2 = browse(client, *it);
      for(ChimeraTK::UASet::iterator it1 = l2.begin(); it1 != l2.end();it1++){
        ChimeraTK::UASet l3 = browse(client, *it1);
      }
    }


    UA_Client_disconnect(client);
    UA_Client_delete(client);
return (int) UA_STATUSCODE_GOOD;
}
