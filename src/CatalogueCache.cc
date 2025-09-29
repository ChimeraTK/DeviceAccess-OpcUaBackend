// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * CatalogueCache.cc
 *
 *  Created on: Sep 18, 2025
 *      Author: Klaus Zenker (HZDR)
 */

#include "CatalogueCache.h"

#include "RegisterInfo.h"

#include <libxml++/libxml++.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <fstream>

namespace ChimeraTK { namespace Cache {
  static std::unique_ptr<xmlpp::DomParser> createDomParser(const std::string& xmlfile);
  static xmlpp::Element* getRootNode(xmlpp::DomParser& parser);
  static unsigned int convertToUint(const std::string& s, int line);
  static int convertToInt(const std::string& s, int line);
  static void parseRegister(
      xmlpp::Element const* registerNode, OpcUaBackendRegisterCatalogue& catalogue, const std::string& serverAddress);
  static unsigned int parseLength(xmlpp::Element const* c);
  static int parseTypeId(xmlpp::Element const* c);
  static ChimeraTK::AccessModeFlags parseAccessMode(xmlpp::Element const* c);
  static void addRegInfoXmlNode(const OpcUABackendRegisterInfo& r, xmlpp::Node* rootNode);

  OpcUaBackendRegisterCatalogue readCatalogue(const std::string& xmlfile) {
    OpcUaBackendRegisterCatalogue catalogue;
    auto parser = createDomParser(xmlfile);
    auto registerList = getRootNode(*parser);

    std::string serverAddress;
    for(auto const node : registerList->get_children()) {
      auto reg = dynamic_cast<const xmlpp::Element*>(node);
      if(reg == nullptr) {
        continue;
      }
      if(reg->get_name() == "general") {
        for(auto& subnode : node->get_children()) {
          auto e = dynamic_cast<const xmlpp::Element*>(subnode);
          if(e == nullptr) {
            continue;
          }
          std::string nodeName = e->get_name();

          if(nodeName == "serverAddress") {
            serverAddress = e->get_child_text()->get_content();
          }
        }
      }
    }
    for(auto const node : registerList->get_children()) {
      auto reg = dynamic_cast<const xmlpp::Element*>(node);
      if(reg == nullptr) {
        continue;
      }
      if(reg->get_name() != "general") parseRegister(reg, catalogue, serverAddress);
    }
    return catalogue;
  }

  bool is_empty(std::ifstream& f) {
    return f.peek() == std::ifstream::traits_type::eof();
  }

  bool is_empty(std::ifstream&& f) {
    return is_empty(f);
  }

  /********************************************************************************************************************/

  void saveCatalogue(const OpcUaBackendRegisterCatalogue& c, const std::string& xmlfile) {
    xmlpp::Document doc;

    auto rootNode = doc.create_root_node("catalogue");
    rootNode->set_attribute("version", "1.0");

    auto commonTag = rootNode->add_child("general");

    auto serverAddressTag = commonTag->add_child("serverAddress");

    bool addressSet = false;
    for(auto& regInfo : c) {
      if(!addressSet) {
        serverAddressTag->set_child_text(static_cast<std::string>(regInfo._serverAddress));
        addressSet = true;
      }
      addRegInfoXmlNode(regInfo, rootNode);
    }

    const std::string pathTemplate = "%%%%%%-opcua-backend-cache-%%%%%%.tmp";
    boost::filesystem::path temporaryName;

    try {
      temporaryName = boost::filesystem::unique_path(boost::filesystem::path(pathTemplate));
    }
    catch(boost::filesystem::filesystem_error& e) {
      throw ChimeraTK::runtime_error(std::string{"Failed to generate temporary path: "} + e.what());
    }

    {
      auto stream = std::ofstream(temporaryName);
      doc.write_to_stream_formatted(stream);
    }

    // check for empty tmp file:
    // xmlpp::Document::write_to_file_formatted sometimes misbehaves on exceptions, creating
    // empty files.
    if(is_empty(std::ifstream(temporaryName))) {
      throw ChimeraTK::runtime_error(std::string{"Failed to save cache File"});
    }

    try {
      boost::filesystem::rename(temporaryName, xmlfile);
    }
    catch(boost::filesystem::filesystem_error& e) {
      throw ChimeraTK::runtime_error(std::string{"Failed to replace cache file: "} + e.what());
    }
  }

  /********************************************************************************************************************/

  void parseRegister(
      xmlpp::Element const* registerNode, OpcUaBackendRegisterCatalogue& catalogue, const std::string& serverAddress) {
    std::string name, description, indexRange;
    bool isReadonly, isNumeric;
    uint32_t typeId;
    uint16_t namespaceId;
    unsigned int length{};
    ChimeraTK::DataDescriptor descriptor{};
    ChimeraTK::AccessModeFlags flags{};

    for(auto& node : registerNode->get_children()) {
      auto e = dynamic_cast<const xmlpp::Element*>(node);
      if(e == nullptr) {
        continue;
      }
      std::string nodeName = e->get_name();

      if(nodeName == "name") {
        name = e->get_child_text()->get_content();
      }
      else if(nodeName == "length") {
        length = parseLength(e);
      }
      else if(nodeName == "access_mode") {
        flags = parseAccessMode(e);
      }
      else if(nodeName == "description") {
        description = e->get_child_text()->get_content();
      }
      else if(nodeName == "isReadonly") {
        isReadonly = (bool)parseTypeId(e);
      }
      else if(nodeName == "typeId") {
        typeId = parseTypeId(e);
      }
      else if(nodeName == "nameSpace") {
        namespaceId = parseTypeId(e);
      }
      else if(nodeName == "isNumeric") {
        isNumeric = (bool)parseTypeId(e);
      }
      else if(nodeName == "indexRange") {
        if(e->has_child_text()) {
          indexRange = e->get_child_text()->get_content();
        }
      }
    }
    if(isNumeric) {
      catalogue.addProperty(UA_NODEID_NUMERIC(namespaceId, std::stoul(name.substr(name.length() - 1))), name,
          indexRange, typeId, length, serverAddress, description, isReadonly);
    }
    else {
      catalogue.addProperty(UA_NODEID_STRING(namespaceId, const_cast<char*>(name.c_str())), name, indexRange, typeId,
          length, serverAddress, description, isReadonly);
    }
  }

  /********************************************************************************************************************/

  unsigned int parseLength(xmlpp::Element const* c) {
    return convertToUint(c->get_child_text()->get_content(), c->get_line());
  }

  /********************************************************************************************************************/

  int parseTypeId(xmlpp::Element const* c) {
    return convertToInt(c->get_child_text()->get_content(), c->get_line());
  }

  /********************************************************************************************************************/

  ChimeraTK::AccessModeFlags parseAccessMode(xmlpp::Element const* c) {
    std::string accessMode{};
    auto t = c->get_child_text();
    if(t != nullptr) {
      accessMode = t->get_content();
    }
    return ChimeraTK::AccessModeFlags::deserialize(accessMode);
  }

  /********************************************************************************************************************/

  std::unique_ptr<xmlpp::DomParser> createDomParser(const std::string& xmlfile) {
    try {
      return std::make_unique<xmlpp::DomParser>(xmlfile);
    }
    catch(std::exception& e) {
      throw ChimeraTK::logic_error("Error opening " + xmlfile + ": " + e.what());
    }
  }

  /********************************************************************************************************************/

  xmlpp::Element* getRootNode(xmlpp::DomParser& parser) {
    try {
      auto root = parser.get_document()->get_root_node();
      if(root->get_name() != "catalogue") {
        ChimeraTK::logic_error("Expected tag 'catalog' got: " + root->get_name());
      }
      return root;
    }
    catch(std::exception& e) {
      throw ChimeraTK::logic_error(e.what());
    }
  }

  /********************************************************************************************************************/

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
  /********************************************************************************************************************/

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

  /********************************************************************************************************************/

  void addRegInfoXmlNode(const OpcUABackendRegisterInfo& r, xmlpp::Node* rootNode) {
    auto registerTag = rootNode->add_child("register");

    auto nameTag = registerTag->add_child("name");
    nameTag->set_child_text(static_cast<std::string>(r.getRegisterName()));

    auto descriptionTag = registerTag->add_child("description");
    descriptionTag->set_child_text(static_cast<std::string>(r._description));

    auto lengthTag = registerTag->add_child("length");
    lengthTag->set_child_text(std::to_string(r.getNumberOfElements()));

    auto accessMode = registerTag->add_child("access_mode");
    accessMode->set_child_text(r._accessModes.serialize());

    auto readOnlyTag = registerTag->add_child("readOnly");
    readOnlyTag->set_child_text(std::to_string(r._isReadonly));

    auto typeTag = registerTag->add_child("typeId");
    typeTag->set_child_text(std::to_string(r._dataType));

    auto nsTag = registerTag->add_child("nameSpace");
    nsTag->set_child_text(std::to_string(r._namespaceIndex));

    auto nodeTypeTag = registerTag->add_child("isNumeric");
    nodeTypeTag->set_child_text(std::to_string(r._isNumeric));

    auto indexRangeTag = registerTag->add_child("indexRange");
    indexRangeTag->set_child_text(static_cast<std::string>(r._indexRange));
  }
}} // namespace ChimeraTK::Cache
