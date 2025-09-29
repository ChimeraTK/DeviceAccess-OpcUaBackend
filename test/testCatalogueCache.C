// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * testCatalogueCache.C
 *
 *  Created on: Sep 19, 2025
 *      Author: Klaus Zenker (HZDR)
 */
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE ServerTest

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "CatalogueCache.h"

BOOST_AUTO_TEST_CASE(testReading) {
  auto cat = ChimeraTK::Cache::readCatalogue("opcua_cache.xml");
  BOOST_CHECK_EQUAL(cat.getNumberOfRegisters(), 6);
}
