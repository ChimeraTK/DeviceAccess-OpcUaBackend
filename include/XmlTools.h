// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
/*
 * XmlTools.hh
 *
 *  Created on: Jan 6, 2026
 *      Author: Klaus Zenker (HZDR)
 */
#include <ChimeraTK/AccessMode.h>

#include <libxml++/libxml++.h>

std::unique_ptr<xmlpp::DomParser> createDomParser(const std::string& xmlfile);
xmlpp::Element* getRootNode(const std::unique_ptr<xmlpp::DomParser>& parser, const std::string& rootNodeName);
unsigned int convertToUint(const std::string& s, int line);
int convertToInt(const std::string& s, int line);
unsigned int parseLength(xmlpp::Element const* c);
int parseTypeId(xmlpp::Element const* c);
ChimeraTK::AccessModeFlags parseAccessMode(xmlpp::Element const* c);
