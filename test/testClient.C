/*
 * testClient.C
 *
 *  Created on: Aug 27, 2018
 *      Author: zenker
 */

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE BackendTest

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
using namespace boost::unit_test_framework;

#include <mtca4u/Device.h>
#include <mtca4u/DeviceException.h>
#include <ChimeraTK-ControlSystemAdapter-OPCUAAdapter/csa_opcua_adapter.h>
#include "ChimeraTK/ControlSystemAdapter/ControlSystemSynchronizationUtility.h"

struct Server{
  std::pair<boost::shared_ptr<ControlSystemPVManager>, boost::shared_ptr<DevicePVManager> > pvManagers;
  boost::shared_ptr<ControlSystemPVManager> csManager;
  boost::shared_ptr<DevicePVManager> devManager;
  boost::shared_ptr<csa_opcua_adapter> csaOPCUA;
  boost::shared_ptr<ControlSystemSynchronizationUtility> syncCsUtility;
  Server() : pvManagers(createPVManager()), csManager(pvManagers.first), devManager(pvManagers.second), csaOPCUA(nullptr){
    syncCsUtility.reset(new ChimeraTK::ControlSystemSynchronizationUtility(csManager));
    syncCsUtility->receiveAll();

    devManager->createProcessArray<int8_t>(controlSystemToDevice, "Scalar/int8", 1, "MV", "Scalar in MV unit");
    devManager->createProcessArray<uint8_t>(controlSystemToDevice, "Scalar/uint8", 1);
    devManager->createProcessArray<int16_t>(controlSystemToDevice, "Scalar/int16", 1);
    devManager->createProcessArray<uint16_t>(controlSystemToDevice, "Scalar/uint16", 1);
    devManager->createProcessArray<int32_t>(controlSystemToDevice, "Scalar/int32", 1, "V", "Scalar in V unit");
    devManager->createProcessArray<uint32_t>(controlSystemToDevice, "Scalar/uint32", 1);
    devManager->createProcessArray<float>(controlSystemToDevice, "Scalar/float", 1);
    devManager->createProcessArray<double>(controlSystemToDevice, "Scalar/doubler", 1);

    devManager->createProcessArray<int8_t>(controlSystemToDevice, "Array/int8_s15", 15, "MV", "Array in MV units");
    devManager->createProcessArray<uint8_t>(controlSystemToDevice, "Array/uint8_s10", 10);
    devManager->createProcessArray<int16_t>(controlSystemToDevice, "Array/int16_s12", 12);
    devManager->createProcessArray<uint16_t>(controlSystemToDevice, "Array/uint16_s10", 10, "V", "Array in V units");
    devManager->createProcessArray<int32_t>(controlSystemToDevice, "Array/int32_s15", 15);
    devManager->createProcessArray<uint32_t>(controlSystemToDevice, "Array/uint32_s10", 10);
    devManager->createProcessArray<double>(controlSystemToDevice, "Array/double_s15", 15);
    devManager->createProcessArray<float>(controlSystemToDevice, "Array/float_s10", 10);


    csaOPCUA.reset(new csa_opcua_adapter(csManager, "test_mapping.xml"));
    // is Server running?
    csaOPCUA->start();
    BOOST_CHECK(csaOPCUA->isRunning() == true);
    // is csManager init
    BOOST_CHECK(csaOPCUA->getControlSystemManager()->getAllProcessVariables().size() == 16);

    BOOST_CHECK(csaOPCUA->getIPCManager() != NULL);

    BOOST_CHECK(csaOPCUA->getUAAdapter() != NULL);
  }
  ~Server(){
    csaOPCUA->stop();
    BOOST_CHECK(csaOPCUA->isRunning() != true);
  }
};


struct testEnvironment{
  testEnvironment(){
  }
  ~testEnvironment() {
  }

};

BOOST_AUTO_TEST_CASE(testWithServer) {
  Server svr;
  sleep(20);

}
