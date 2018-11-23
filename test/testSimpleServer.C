/*
 * testSimpleServer.C
 *
 *  Created on: Nov 19, 2018
 *      Author: zenker
 */


#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE ServerTest

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
using namespace boost::unit_test_framework;

#include <signal.h>
#include "open62541.h"
#include <thread>
#include <chrono>

UA_Boolean running = true;

struct Server{
  Server(){
    UA_ServerConfig config = UA_ServerConfig_standard;
    nl = UA_ServerNetworkLayerTCP(UA_ConnectionConfig_standard, 4840);
    config.networkLayers = &nl;
    config.networkLayersSize = 1;
    server = UA_Server_new(config);
  }

  ~Server(){
    UA_Server_delete(server);
    nl.deleteMembers(&nl);
  }

  UA_Server *server;
  UA_ServerNetworkLayer nl;

  void start(){
    UA_Server_run(server, &running);
  }

  /*
   * Stop the server by setting running to false
   * This can be done by a signal handler or by setting running to false from outside -
   * so no interaction with the server is needed...
   */
  void stop(){
    running = false;
  }

  void addVariable() {
      /* Define the attribute of the myInteger variable node */
      UA_VariableAttributes attr;
      UA_VariableAttributes_init(&attr);
      UA_Int32 myInteger = 42;
      UA_Variant_setScalar(&attr.value, &myInteger, &UA_TYPES[UA_TYPES_INT32]);
      attr.description = UA_LOCALIZEDTEXT("en_US","the answer");
      attr.displayName = UA_LOCALIZEDTEXT("en_US","the answer");
      attr.dataType = UA_TYPES[UA_TYPES_INT32].typeId;

      /* Add the variable node to the information model */
      UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, "the.answer");
      UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, "the answer");
      UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
      UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
      UA_Server_addVariableNode(server, myIntegerNodeId, parentNodeId,
                                parentReferenceNodeId, myIntegerName,
                                UA_NODEID_NULL, attr, NULL, NULL);
  }

  void
  writeVariableA() {
      UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, "the.answer");

      /* Write a different integer value */
      UA_Int32 myInteger = 43;
      UA_Variant myVar;
      UA_Variant_init(&myVar);
      UA_Variant_setScalar(&myVar, &myInteger, &UA_TYPES[UA_TYPES_INT32]);
      UA_Server_writeValue(server, myIntegerNodeId, myVar);

      /* Set the status code of the value to an error code. The function
       * UA_Server_write provides access to the raw service. The above
       * UA_Server_writeValue is syntactic sugar for writing a specific node
       * attribute with the write service. */
      UA_WriteValue wv;
      UA_WriteValue_init(&wv);
      wv.nodeId = myIntegerNodeId;
      wv.attributeId = UA_ATTRIBUTEID_VALUE;
      wv.value.status = UA_STATUSCODE_BADNOTCONNECTED;
      wv.value.hasStatus = true;
      UA_Server_write(server, &wv);

      /* Reset the variable to a good statuscode with a value */
      wv.value.hasStatus = false;
      wv.value.value = myVar;
      wv.value.hasValue = true;
      UA_Server_write(server, &wv);
  }

  void
  writeVariableB() {
      UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, "the.answer");

      /* Write a different integer value */
      UA_Int32 myInteger = 42;
      UA_Variant myVar;
      UA_Variant_init(&myVar);
      UA_Variant_setScalar(&myVar, &myInteger, &UA_TYPES[UA_TYPES_INT32]);

      /* Set the status code of the value to an error code. The function
       * UA_Server_write provides access to the raw service. The above
       * UA_Server_writeValue is syntactic sugar for writing a specific node
       * attribute with the write service. */
      UA_WriteValue wv;
      UA_WriteValue_init(&wv);
      wv.nodeId = myIntegerNodeId;
      wv.attributeId = UA_ATTRIBUTEID_VALUE;
      wv.value.status = UA_STATUSCODE_BADNOTCONNECTED;
      wv.value.hasStatus = true;
      UA_Server_write(server, &wv);

      /* Reset the variable to a good statuscode with a value */
      wv.value.hasStatus = false;
      wv.value.value = myVar;
      wv.value.hasValue = true;
      UA_Server_write(server, &wv);
  }

};

Server* svr;

void run(){
  svr = new Server();
  svr->addVariable();
  svr->start();
  delete svr;
}

BOOST_AUTO_TEST_CASE(testWithServer) {
  // this thread is blocked by the server
  std::thread t1(run);
  // wait
  std::this_thread::sleep_for(std::chrono::seconds(10));
  svr->writeVariableA();
  std::this_thread::sleep_for(std::chrono::seconds(2));
  svr->writeVariableB();
  std::this_thread::sleep_for(std::chrono::seconds(5));
  // stop server - could also use svr->stop();
  running = false;
  // join the server thread
  t1.join();
}
