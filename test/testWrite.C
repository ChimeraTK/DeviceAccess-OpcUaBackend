// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * testMapFiles.C
 *
 *  Created on: Jan 26, 2026
 *      Author: Klaus Zenker (HZDR)
 *
 *  This test was added to run valgrind, which does not work for the UnifiedBackendTest.
 *  The functionality of the test is also tested by the UnifiedBackendTest.
 */

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE ServerTest

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "ChimeraTK/Device.h"
#include "DummyServer.h"

#include <chrono>
#include <iostream>
#include <thread>

void printReg(const ChimeraTK::TwoDRegisterAccessor<std::string>& reg) {
  std::cout << "Register values:\n\t" << std::endl;
  for(auto val : reg[0]) {
    std::cout << "\t" << val.c_str() << std::endl;
  }
}

BOOST_AUTO_TEST_CASE(testWrite) {
  ThreadedOPCUAServer dummy;
  dummy.start();
  BOOST_CHECK_EQUAL(true, dummy.checkConnection(ServerState::On));
  std::stringstream ss;
  ss << "(opcua:localhost?port=" << dummy.server.getPort() << ")";
  ChimeraTK::Device d(ss.str());
  d.open();
  BOOST_CHECK_EQUAL(true, d.isFunctional());
  BOOST_CHECK_EQUAL(true, d.isOpened());
  auto reg = d.getTwoDRegisterAccessor<std::string>("Dummy/array/string");
  BOOST_CHECK_NO_THROW(reg.read());
  printReg(reg);
  reg[0][2] = std::string("new value at 2");
  BOOST_CHECK_NO_THROW(reg.write());
  printReg(reg);
  reg[0][1] = std::string("new value at 1");
  reg[0][3] = std::string("new value at 3");
  BOOST_CHECK_NO_THROW(reg.write());
  printReg(reg);

  auto reg1 = d.getTwoDRegisterAccessor<std::string>("Dummy/array/string", 2, 1);
  BOOST_CHECK_NO_THROW(reg1.read());
  printReg(reg1);
  BOOST_CHECK_EQUAL(std::string("new value at 1"), reg1[0][0]);
  reg1[0][1] = std::string("new value at 2 from partial write");
  BOOST_CHECK_NO_THROW(reg1.write());
  printReg(reg1);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  BOOST_CHECK_NO_THROW(reg.read());
  printReg(reg);
}
