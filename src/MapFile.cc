// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * MapFile.cc
 *
 *  Created on: Aug 7, 2024
 *      Author: Klaus Zenker (HZDR)
 */

#include "MapFile.h"

#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/xpathInternals.h>

#include <boost/algorithm/string.hpp>

namespace ChimeraTK {
  OPCUAMapFileReader::OPCUAMapFileReader(const std::string& filePath, const std::string& rootNode)
  : _file(filePath), _rootNode(rootNode) {
    doc = xmlReadFile(filePath.c_str(), nullptr, XML_PARSE_NOERROR);
    if(!doc) {
      throw ChimeraTK::runtime_error(std::string("OPC UA device failed parsing map file ") + filePath);
    }
    readElements();
  }

  xmlXPathObjectPtr OPCUAMapFileReader::getNodeSet(const std::string& xPathStr) {
    auto* xpath = (xmlChar*)xPathStr.c_str();
    xmlXPathContextPtr context;
    xmlXPathObjectPtr result;

    context = xmlXPathNewContext(doc);
    if(context == nullptr) {
      return nullptr;
    }
    if(xmlXPathRegisterNs(
           context, (xmlChar*)"csa", (xmlChar*)"https://github.com/ChimeraTK/DeviceAccess-OpcUaBackend") != 0) {
      throw ChimeraTK::runtime_error(
          "Failed to register xml namespace: https://github.com/ChimeraTK/DeviceAccess-OpcUaBackend");
    }

    result = xmlXPathEvalExpression(xpath, context);
    xmlXPathFreeContext(context);
    if(result == nullptr) {
      return nullptr;
    }
    if(xmlXPathNodeSetIsEmpty(result->nodesetval)) {
      xmlXPathFreeObject(result);
      return nullptr;
    }
    return result;
  }

  std::string OPCUAMapFileReader::getAttributeValueFromNode(xmlNode* node, const std::string& attributeName) {
    xmlAttrPtr attr = xmlHasProp(node, (xmlChar*)attributeName.c_str());
    if(attr != nullptr) {
      std::string merker = (std::string)((char*)attr->children->content);
      return merker;
    }
    return "";
  }

  void OPCUAMapFileReader::readElements() {
    auto result = getNodeSet("//pv");
    if(result) {
      auto nodeset = result->nodesetval;
      for(size_t i = 0; i < nodeset->nodeNr; i++) {
        std::string nsString, name, range, node;
        auto content = xmlNodeGetContent(nodeset->nodeTab[i]->xmlChildrenNode);
        if(content != nullptr) {
          node = (std::string)((char*)content);
          xmlFree(content);
          boost::algorithm::trim(node);
        }
        nsString = getAttributeValueFromNode(nodeset->nodeTab[i], "ns");
        range = getAttributeValueFromNode(nodeset->nodeTab[i], "range");
        name = getAttributeValueFromNode(nodeset->nodeTab[i], "name");
        try {
          UA_UInt32 id = std::stoul(node);
          UA_UInt16 ns = std::stoul(nsString);
          _elements.push_back(MapElement(id, ns, range, name));
        }
        catch(std::invalid_argument& e) {
          try {
            UA_UInt16 ns = std::stoul(nsString);
            _elements.push_back(MapElement(_rootNode + node, ns, range, name));
          }
          catch(std::invalid_argument& innerError) {
            UA_LOG_ERROR(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
                "Failed reading line %d from opcua map file %s.", nodeset->nodeTab[i]->line, _file.c_str());
          }
          catch(std::out_of_range& e) {
            UA_LOG_ERROR(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
                "Failed reading the line %d from mapping file %s (Namespace id is out of range!).",
                nodeset->nodeTab[i]->line, _file.c_str());
          }
        }
        catch(std::out_of_range& e) {
          UA_LOG_ERROR(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
              "Failed reading line %d from mapping file %s (Namespace id or Node id is out of range!).",
              nodeset->nodeTab[i]->line, _file.c_str());
        }
      }
    }
  }

  MapElement::MapElement(const UA_UInt32& id, const UA_UInt16& ns, const std::string& range, const std::string& name)
  : _iNode(id), _namespace(ns), _node(UA_NODEID_NUMERIC(ns, id)), _range(range), _name(name) {}
  MapElement::MapElement(const std::string& id, const UA_UInt16& ns, const std::string& range, const std::string& name)
  : _strNode(id), _namespace(ns), _node(UA_NODEID_STRING(ns, const_cast<char*>(_strNode.c_str()))), _range(range),
    _name(name) {}

} // namespace ChimeraTK
