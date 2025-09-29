// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

/*
 * CatalogueCache.h
 *
 *  Created on: Sep 18, 2025
 *      Author: Klaus Zenker (HZDR)
 */

#include "RegisterInfo.h"

#include <ChimeraTK/BackendRegisterCatalogue.h>

namespace ChimeraTK { namespace Cache {
  OpcUaBackendRegisterCatalogue readCatalogue(const std::string& xmlfile);
  void saveCatalogue(const OpcUaBackendRegisterCatalogue& c, const std::string& xmlfile);
}} // namespace ChimeraTK::Cache
