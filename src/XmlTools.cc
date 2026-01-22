// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * XmlTools.cc
 *
 *  Created on: Jan 6, 2026
 *      Author: Klaus Zenker (HZDR)
 */

#include "XmlTools.h"

#include <ChimeraTK/Exception.h>

unsigned int parseLength(xmlpp::Element const* c) {
  return convertToUint(c->get_child_text()->get_content(), c->get_line());
}

int parseTypeId(xmlpp::Element const* c) {
  return convertToInt(c->get_child_text()->get_content(), c->get_line());
}

ChimeraTK::AccessModeFlags parseAccessMode(xmlpp::Element const* c) {
  std::string accessMode{};
  const auto* t = c->get_child_text();
  if(t != nullptr) {
    accessMode = t->get_content();
  }
  return ChimeraTK::AccessModeFlags::deserialize(accessMode);
}

std::unique_ptr<xmlpp::DomParser> createDomParser(const std::string& xmlfile) {
  try {
    return std::make_unique<xmlpp::DomParser>(xmlfile);
  }
  catch(std::exception& e) {
    throw ChimeraTK::logic_error("Error opening " + xmlfile + ": " + e.what());
  }
}

xmlpp::Element* getRootNode(const std::unique_ptr<xmlpp::DomParser>& parser, const std::string& rootNodeName) {
  try {
    auto root = parser->get_document()->get_root_node();
    if(root->get_name() != rootNodeName) {
      ChimeraTK::logic_error(std::string("Expected tag '") + rootNodeName + "' got: " + root->get_name());
    }

    auto namespaceStr = root->get_namespace_uri();
    if(namespaceStr.compare("https://github.com/ChimeraTK/DeviceAccess-OpcUaBackend") != 0) {
      throw ChimeraTK::logic_error(std::string("Wrong namespace is used: ") + namespaceStr.c_str() +
          "/t -> Should be 'https://github.com/ChimeraTK/DeviceAccess-OpcUaBackend'");
    }

    return root;
  }
  catch(std::exception& e) {
    throw ChimeraTK::logic_error(e.what());
  }
}

unsigned int convertToUint(const std::string& s, int line) {
  try {
    return std::stoul(s);
  }
  catch(std::invalid_argument& e) {
    throw ChimeraTK::logic_error("Failed to parse node at line " + std::to_string(line) + ":" + e.what());
  }
  catch(std::out_of_range& e) {
    throw ChimeraTK::logic_error("Failed to parse node at line " + std::to_string(line) + ":" + e.what());
  }
}

int convertToInt(const std::string& s, int line) {
  try {
    return std::stol(s);
  }
  catch(std::invalid_argument& e) {
    throw ChimeraTK::logic_error("Failed to parse node at line " + std::to_string(line) + ":" + e.what());
  }
  catch(std::out_of_range& e) {
    throw ChimeraTK::logic_error("Failed to parse node at line " + std::to_string(line) + ":" + e.what());
  }
}
