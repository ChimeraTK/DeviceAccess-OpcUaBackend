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
#include "XmlTools.h"

#include <libxml++/libxml++.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <fstream>

namespace ChimeraTK::Cache {
  static void parseRegister(
      xmlpp::Element const* registerNode, OpcUaBackendRegisterCatalogue& catalogue, const std::string& serverAddress);
  static void addRegInfoXmlNode(const OpcUABackendRegisterInfo& r, xmlpp::Node* rootNode);

  OpcUaBackendRegisterCatalogue readCatalogue(const std::string& xmlfile) {
    OpcUaBackendRegisterCatalogue catalogue;
    auto parser = createDomParser(xmlfile);
    auto* registerList = getRootNode(parser, "catalogue");

    std::string serverAddress;
    for(auto* const node : registerList->get_children()) {
      const auto* reg = dynamic_cast<const xmlpp::Element*>(node);
      if(reg == nullptr) {
        continue;
      }
      if(reg->get_name() == "general") {
        for(auto& subnode : node->get_children()) {
          const auto* e = dynamic_cast<const xmlpp::Element*>(subnode);
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
    for(auto* const node : registerList->get_children()) {
      const auto* reg = dynamic_cast<const xmlpp::Element*>(node);
      if(reg == nullptr) {
        continue;
      }
      if(reg->get_name() != "general") {
        parseRegister(reg, catalogue, serverAddress);
      }
    }
    return catalogue;
  }

  bool isEmpty(std::ifstream& f) {
    return f.peek() == std::ifstream::traits_type::eof();
  }

  bool isEmpty(std::ifstream&& f) {
    return isEmpty(f);
  }

  /********************************************************************************************************************/

  void saveCatalogue(const OpcUaBackendRegisterCatalogue& c, const std::string& xmlfile) {
    xmlpp::Document doc;

    auto* rootNode = doc.create_root_node("catalogue", "https://github.com/ChimeraTK/DeviceAccess-OpcUaBackend", "ctk");
    rootNode->set_attribute("version", "1.0");

    auto* commonTag = rootNode->add_child("general");

    auto* serverAddressTag = commonTag->add_child("serverAddress");

    bool addressSet = false;
    for(const auto& regInfo : c) {
      if(!addressSet) {
        serverAddressTag->set_child_text(static_cast<std::string>(regInfo.serverAddress));
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
    if(isEmpty(std::ifstream(temporaryName))) {
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
    std::string nodeId, name, description, indexRange;
    bool isReadonly, isNumeric;
    uint32_t typeId;
    uint16_t namespaceId;
    unsigned int length{};
    ChimeraTK::DataDescriptor descriptor{};
    ChimeraTK::AccessModeFlags flags{};

    for(const auto& node : registerNode->get_children()) {
      const auto* e = dynamic_cast<const xmlpp::Element*>(node);
      if(e == nullptr) {
        continue;
      }
      std::string nodeName = e->get_name();

      if(nodeName == "name") {
        name = e->get_child_text()->get_content();
      }
      else if(nodeName == "nodeId") {
        nodeId = e->get_child_text()->get_content();
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
      else if(nodeName == "readOnly") {
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
      catalogue.addProperty(UA_NODEID_NUMERIC(namespaceId, std::stoul(nodeId.substr(nodeId.length() - 1))), name,
          indexRange, typeId, length, serverAddress, description, isReadonly);
    }
    else {
      catalogue.addProperty(UA_NODEID_STRING(namespaceId, const_cast<char*>(nodeId.c_str())), name, indexRange, typeId,
          length, serverAddress, description, isReadonly);
    }
  }

  /********************************************************************************************************************/

  void addRegInfoXmlNode(const OpcUABackendRegisterInfo& r, xmlpp::Node* rootNode) {
    auto* registerTag = rootNode->add_child("register");

    auto* nodeIdTag = registerTag->add_child("nodeId");
    std::string name;
    if(r.isNumeric) {
      name = std::to_string(r.id.identifier.numeric);
    }
    else {
      name = std::string((char*)r.id.identifier.string.data, r.id.identifier.string.length);
    }
    nodeIdTag->set_child_text(name);

    auto* nameTag = registerTag->add_child("name");
    nameTag->set_child_text((std::string)r.getRegisterName());

    auto* descriptionTag = registerTag->add_child("description");
    descriptionTag->set_child_text(static_cast<std::string>(r.description));

    auto* lengthTag = registerTag->add_child("length");
    lengthTag->set_child_text(std::to_string(r.getNumberOfElements()));

    auto* accessMode = registerTag->add_child("access_mode");
    accessMode->set_child_text(r.accessModes.serialize());

    auto* readOnlyTag = registerTag->add_child("readOnly");
    readOnlyTag->set_child_text(std::to_string(r.isReadonly));

    auto* typeTag = registerTag->add_child("typeId");
    typeTag->set_child_text(std::to_string(r.dataType));

    auto* nsTag = registerTag->add_child("nameSpace");
    nsTag->set_child_text(std::to_string(r.namespaceIndex));

    auto* nodeTypeTag = registerTag->add_child("isNumeric");
    nodeTypeTag->set_child_text(std::to_string(r.isNumeric));

    auto* indexRangeTag = registerTag->add_child("indexRange");
    indexRangeTag->set_child_text(static_cast<std::string>(r.indexRange));
  }
} // namespace ChimeraTK::Cache
