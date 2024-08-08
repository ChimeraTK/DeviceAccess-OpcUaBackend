// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * testMapFiles.C
 *
 *  Created on: Aug 8, 2024
 *      Author: Klaus Zenker (HZDR)
 */

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE ServerTest

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "ChimeraTK/Device.h"
#include "DummyServer.h"

#include <chrono>
#include <sstream>
#include <thread>

BOOST_AUTO_TEST_CASE(testBrowsing) {
  ThreadedOPCUAServer dummy;
  dummy.start();
  BOOST_CHECK_EQUAL(true, dummy.checkConnection(ServerState::On));
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::stringstream ss;
  ss << "(opcua:localhost?port=" << dummy._server.getPort() << ")";
  ChimeraTK::Device d(ss.str());
  d.open();
  BOOST_CHECK_EQUAL(true, d.isFunctional());
  BOOST_CHECK_EQUAL(true, d.isOpened());
  auto reg = d.getTwoDRegisterAccessor<int>("Dummy/scalar/int32");
  BOOST_CHECK_NO_THROW(reg.read());
}

BOOST_AUTO_TEST_CASE(testMapFileLegacy) {
  ThreadedOPCUAServer dummy;
  dummy.start();
  BOOST_CHECK_EQUAL(true, dummy.checkConnection(ServerState::On));
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::stringstream ss;
  ss << "(opcua:localhost?port=" << dummy._server.getPort() << "&map=opcua_map.map)";
  ChimeraTK::Device d(ss.str());
  d.open();
  BOOST_CHECK_EQUAL(true, d.isFunctional());
  BOOST_CHECK_EQUAL(true, d.isOpened());
  auto reg = d.getTwoDRegisterAccessor<int>("Dummy/scalar/int32");
  BOOST_CHECK_NO_THROW(reg.read());
  auto regNewName = d.getTwoDRegisterAccessor<int>("Test/newName");
  BOOST_CHECK_NO_THROW(regNewName.read());
}

BOOST_AUTO_TEST_CASE(testMapFileSync) {
  ThreadedOPCUAServer dummy;
  dummy.start();
  BOOST_CHECK_EQUAL(true, dummy.checkConnection(ServerState::On));
  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::vector<int> v{1, 2, 3, 4, 5};
  dummy._server.setValue("Dummy/array/int32", v, 5);
  std::stringstream ss;
  ss << "(opcua:localhost?port=" << dummy._server.getPort() << "&map=opcua_map_xml.map)";
  ChimeraTK::Device d(ss.str());
  d.open();
  BOOST_CHECK_EQUAL(true, d.isFunctional());
  BOOST_CHECK_EQUAL(true, d.isOpened());
  auto reg = d.getTwoDRegisterAccessor<int>("Dummy/scalar/int32");
  BOOST_CHECK_NO_THROW(reg.read());
  auto regNewName = d.getTwoDRegisterAccessor<int>("Test/newName");
  BOOST_CHECK_NO_THROW(regNewName.read());
  auto regLongArray = d.getTwoDRegisterAccessor<int>("Test/newNameArrayLong");
  BOOST_CHECK_NO_THROW(regLongArray.read());
  BOOST_CHECK_EQUAL(5, regLongArray.getNElementsPerChannel());
  for(size_t i = 0; i < 5; i++) {
    BOOST_CHECK_EQUAL(v.at(i), regLongArray[0][i]);
  }
  auto regShortArray = d.getTwoDRegisterAccessor<int>("Test/newNameArray");
  BOOST_CHECK_NO_THROW(regShortArray.read());
  BOOST_CHECK_EQUAL(3, regShortArray.getNElementsPerChannel());
  for(size_t i = 0; i < 3; i++) {
    BOOST_CHECK_EQUAL(regShortArray[0][i], 3 + i);
  }
  auto regArraySingleElement = d.getTwoDRegisterAccessor<int>("Test/newNameArraySingleElement");
  BOOST_CHECK_NO_THROW(regArraySingleElement.read());
  BOOST_CHECK_EQUAL(1, regArraySingleElement.getNElementsPerChannel());
  BOOST_CHECK_EQUAL(regArraySingleElement[0][0], 3);
}

BOOST_AUTO_TEST_CASE(testMapFileAsync) {
  ThreadedOPCUAServer dummy;
  dummy.start();
  BOOST_CHECK_EQUAL(true, dummy.checkConnection(ServerState::On));
  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::vector<int> v{1, 2, 3, 4, 5};
  dummy._server.setValue("Dummy/array/int32", v, 5);
  std::stringstream ss;
  ss << "(opcua:localhost?port=" << dummy._server.getPort() << "&map=opcua_map_xml.map)";
  ChimeraTK::Device d(ss.str());
  d.open();
  d.activateAsyncRead();
  BOOST_CHECK_EQUAL(true, d.isFunctional());
  BOOST_CHECK_EQUAL(true, d.isOpened());
  auto reg = d.getTwoDRegisterAccessor<int>("Dummy/scalar/int32");
  BOOST_CHECK_NO_THROW(reg.read());
  auto regNewName = d.getTwoDRegisterAccessor<int>("Test/newName");
  BOOST_CHECK_NO_THROW(regNewName.read());
  auto regLongArray = d.getTwoDRegisterAccessor<int>("Test/newNameArrayLong");
  BOOST_CHECK_NO_THROW(regLongArray.read());
  BOOST_CHECK_EQUAL(5, regLongArray.getNElementsPerChannel());
  for(size_t i = 0; i < 5; i++) {
    BOOST_CHECK_EQUAL(v.at(i), regLongArray[0][i]);
  }
  auto regShortArray = d.getTwoDRegisterAccessor<int>("Test/newNameArray");
  BOOST_CHECK_NO_THROW(regShortArray.read());
  BOOST_CHECK_EQUAL(3, regShortArray.getNElementsPerChannel());
  for(size_t i = 0; i < 3; i++) {
    BOOST_CHECK_EQUAL(regShortArray[0][i], 3 + i);
  }
  auto regArraySingleElement = d.getTwoDRegisterAccessor<int>("Test/newNameArraySingleElement");
  BOOST_CHECK_NO_THROW(regArraySingleElement.read());
  BOOST_CHECK_EQUAL(1, regArraySingleElement.getNElementsPerChannel());
  BOOST_CHECK_EQUAL(regArraySingleElement[0][0], 3);
}
