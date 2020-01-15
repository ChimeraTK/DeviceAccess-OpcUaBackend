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

static void
browseRecursive(UA_Client *client, UA_NodeId startingNode, UA_UInt32 nodeClassMask, UA_ReferenceDescription **resultSet, UA_UInt32 *resultSize) {
    UA_BrowseDescription *bd = UA_BrowseDescription_new();
    UA_BrowseDescription_init(bd);
    bd->nodeId = startingNode;
    bd->browseDirection = UA_BROWSEDIRECTION_FORWARD;
    bd->resultMask = UA_BROWSERESULTMASK_ALL;
//    bd->nodeClassMask = UA_NODECLASS_VARIABLE; //search only variable nodes

    UA_BrowseRequest browseRequest;
    UA_BrowseRequest_init(&browseRequest);
    browseRequest.nodesToBrowse = bd;
    browseRequest.nodesToBrowseSize = 1;

    UA_BrowseResponse brp = UA_Client_Service_browse(client, browseRequest);
    if(brp.resultsSize > 0){
        for(size_t i = 0; i < brp.results[0].referencesSize; i++){
            if(brp.results[0].references[i].nodeClass == UA_NODECLASS_OBJECT){
                browseRecursive(client, brp.results[0].references[i].nodeId.nodeId, nodeClassMask, resultSet, resultSize);
            }
            if(brp.results[0].references[i].nodeClass == nodeClassMask){
                *resultSet = (UA_ReferenceDescription *) UA_realloc(*resultSet, ((*resultSize)+1) * sizeof(UA_ReferenceDescription));
                UA_ReferenceDescription_copy(&brp.results[0].references[i], &((*resultSet)[*resultSize]));
                (*resultSize)++;
            }
        }
    }
    bd->nodeId = UA_NODEID_NULL;
    UA_BrowseRequest_deleteMembers(&browseRequest);
    UA_BrowseResponse_deleteMembers(&brp);
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

    /*** Andreas Begin***/
    UA_UInt32 *resultCounter = UA_UInt32_new();
    UA_ReferenceDescription *referenceDescription = UA_ReferenceDescription_new();
//    browseRecursive(client, UA_NODEID_NUMERIC(1, 166), UA_NODECLASS_VARIABLE, &referenceDescription, resultCounter);
    browseRecursive(client, UA_NODEID_NUMERIC(0,UA_NS0ID_OBJECTSFOLDER), UA_NODECLASS_VARIABLE, &referenceDescription, resultCounter);
    printf("Result size %u\n", *resultCounter);
    for(size_t i = 0; i < *resultCounter; i++){
        printf("%s\n", referenceDescription[i].browseName.name.data);
        if(referenceDescription[i].nodeId.nodeId.identifierType == UA_NODEIDTYPE_STRING) {
          printf("NS-Index: %-9d NodeID: %-16.*s Browsename: %-16.*s Displayname: %-16.*s \n", referenceDescription[i].nodeId.nodeId.namespaceIndex,
                               (int)referenceDescription[i].nodeId.nodeId.identifier.string.length,
                               referenceDescription[i].nodeId.nodeId.identifier.string.data,
                               (int)referenceDescription[i].browseName.name.length, referenceDescription[i].browseName.name.data,
                               (int)referenceDescription[i].displayName.text.length, referenceDescription[i].displayName.text.data);

          printf("ServerIndex %d Namespace: %-9d Type: %d\n", referenceDescription[i].typeDefinition.serverIndex,referenceDescription[i].typeDefinition.nodeId.namespaceIndex,
              referenceDescription[i].typeDefinition.nodeId.identifier.numeric);



          UA_ExpandedNodeId id;
          UA_NodeId* idd = UA_NodeId_new();
          UA_Client_readDataTypeAttribute(client,referenceDescription[i].nodeId.nodeId,idd);

          UA_LocalizedText* text = UA_LocalizedText_new();
          UA_Client_readDescriptionAttribute(client,referenceDescription[i].nodeId.nodeId,text);
          printf("%-9d \t %-16.*s", idd->identifier.numeric, text->text.length, text->text.data);
          UA_LocalizedText_deleteMembers(text);
        }
        UA_ReferenceDescription_deleteMembers(&referenceDescription[i]);
    }
    UA_UInt32_delete(resultCounter);
    UA_free(referenceDescription);
    /*** Andreas End***/

    UA_Client_disconnect(client);
    UA_Client_delete(client);
return (int) UA_STATUSCODE_GOOD;
}
