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
#include <open62541/server_config_default.h>
#include <thread>
#include <chrono>

UA_Boolean running = true;

struct Server{
  Server(){
    server = UA_Server_new();
    UA_ServerConfig_setMinimal(UA_Server_getConfig(server), 4840, NULL);
  }

  ~Server(){
    UA_Server_delete(server);
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
      UA_VariableAttributes attr = UA_VariableAttributes_default;
      UA_Int32 myInteger = 42;
      UA_Variant_setScalar(&attr.value, &myInteger, &UA_TYPES[UA_TYPES_INT32]);
      attr.description = UA_LOCALIZEDTEXT_ALLOC("en_US","the answer");
      attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en_US","the answer");
      attr.dataType = UA_TYPES[UA_TYPES_INT32].typeId;

      /* Add the variable node to the information model */
      UA_NodeId myIntegerNodeId = UA_NODEID("ns=1;s=the.answer");
      UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME_ALLOC(1, "the answer");
      UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
      UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
      UA_Server_addVariableNode(server, myIntegerNodeId, parentNodeId,
                                parentReferenceNodeId, myIntegerName,
                                UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);
  }

  void
  writeVariableA() {
      UA_NodeId myIntegerNodeId = UA_NODEID("ns1;s=the.answer");

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
      UA_NodeId myIntegerNodeId = UA_NODEID("ns=1;s=the.answer");

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

//void run(){
//  svr = new Server();
//  svr->addVariable();
//  svr->start();
//  delete svr;
//}

void run1(){
  svr = new Server();
  svr->addVariable();
  svr->start();
}

void run2(){
//  svr->addVariable();
  svr->start();
  delete svr;
}


BOOST_AUTO_TEST_CASE(testWithServer) {
  // this thread is blocked by the server
  std::thread t1(run1);
  // wait
  std::this_thread::sleep_for(std::chrono::seconds(1));
  svr->writeVariableA();
  std::this_thread::sleep_for(std::chrono::seconds(2));
  svr->writeVariableB();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  // stop server - could also use svr->stop();
  running = false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  // join the server thread
  t1.join();

  std::thread t2(run2);
  std::this_thread::sleep_for(std::chrono::seconds(5));
  running = false;
  t2.join();

}

#include "DummyServer.h"
BOOST_AUTO_TEST_CASE(testDummyServer){
  ThreadedOPCUAServer dummy;
  dummy.start();
  BOOST_CHECK_EQUAL(true,dummy.checkConnection(ServerState::On));
  std::this_thread::sleep_for(std::chrono::seconds(5));
}
