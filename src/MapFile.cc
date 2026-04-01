// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * MapFile.cc
 *
 *  Created on: Aug 7, 2024
 *      Author: Klaus Zenker (HZDR)
 */

#include "MapFile.h"

#include "XmlTools.h"

#include <libxml++/libxml++.h>

#include <boost/algorithm/string.hpp>

#include <utility>

namespace ChimeraTK {
  OPCUAMapFileReader::OPCUAMapFileReader(const std::string& filePath, const std::string& rootNode)
  : _file(filePath), _serverRootNode(rootNode) {
    auto parser = createDomParser(filePath);
    _rootNode = getRootNode(parser, "opcua_map");
    readElements();
    if(elements.empty()) {
      UA_LOG_ERROR(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
          "No valid entries found in opcua map file %s.", filePath.c_str());
    }
  }

  void OPCUAMapFileReader::readElements() {
    for(auto* const i : _rootNode->get_children()) {
      std::string nsString, name, range, node;
      const auto* reg = dynamic_cast<const xmlpp::Element*>(i);
      if(reg == nullptr) {
        continue;
      }
      if(reg->get_name() == "pv") {
        node = reg->get_child_text()->get_content();
        auto* nsAttribute = reg->get_attribute("ns");
        if(nsAttribute) {
          nsString = nsAttribute->get_value();
        }
        auto* nameAttribute = reg->get_attribute("name");
        if(nameAttribute) {
          name = nameAttribute->get_value();
        }
        auto* rangeAttribute = reg->get_attribute("range");
        if(rangeAttribute) {
          range = rangeAttribute->get_value();
        }
        try {
          UA_UInt32 id = std::stoul(node);
          UA_UInt16 ns = std::stoul(nsString);
          elements.emplace_back(MapElement(id, ns, range, name));
        }
        catch(std::invalid_argument& e) {
          try {
            UA_UInt16 ns = std::stoul(nsString);
            elements.emplace_back(MapElement(_serverRootNode + node, ns, range, name));
          }
          catch(std::invalid_argument& innerError) {
            UA_LOG_ERROR(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
                "Failed reading line %d from opcua map file %s.", reg->get_line(), _file.c_str());
          }
          catch(std::out_of_range& e) {
            UA_LOG_ERROR(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
                "Failed reading the line %d from mapping file %s (Namespace id is out of range!).", reg->get_line(),
                _file.c_str());
          }
        }
        catch(std::out_of_range& e) {
          UA_LOG_ERROR(&OpcUABackend::backendLogger, UA_LOGCATEGORY_USERLAND,
              "Failed reading line %d from mapping file %s (Namespace id or Node id is out of range!).",
              reg->get_line(), _file.c_str());
        }
      }
    }
  }

  MapElement::MapElement(const UA_UInt32& id, const UA_UInt16& ns, const std::string& range, const std::string& name)
  : _iNode(id), _namespace(ns), _node(UA_NODEID_NUMERIC(ns, id)), _range(range), _name(name) {}
  MapElement::MapElement(const std::string& id, const UA_UInt16& ns, const std::string& range, const std::string& name)
  : _strNode(id), _namespace(ns), _range(range), _name(name) {
    _node = UA_NODEID_STRING_ALLOC(ns, _strNode.c_str());
  }

  MapElement::~MapElement() {
    UA_NodeId_clear(&_node);
  }

  MapElement::MapElement(const MapElement& other)
  : _namespace(other._namespace), _strNode(other._strNode), _iNode(other._iNode), _range(other._range),
    _name(other._name) {
    UA_NodeId_copy(&other._node, &_node);
  }

  MapElement& MapElement::operator=(const MapElement& other) {
    if(this != &other) {
      UA_NodeId_clear(&_node);
      _namespace = other._namespace;
      _strNode = other._strNode;
      _iNode = other._iNode;
      _range = other._range;
      _name = other._name;
      UA_NodeId_copy(&other._node, &_node);
    }
    return *this;
  }

  MapElement::MapElement(MapElement&& other) noexcept
  : _namespace(other._namespace), _strNode(std::move(other._strNode)), _iNode(other._iNode), _node(other._node),
    _range(std::move(other._range)), _name(std::move(other._name)) {
    UA_NodeId_init(&other._node); // Reset source so it won't free our data
  }

  MapElement& MapElement::operator=(MapElement&& other) noexcept {
    if(this != &other) {
      UA_NodeId_clear(&_node);
      _namespace = other._namespace;
      _strNode = std::move(other._strNode);
      _iNode = other._iNode;
      _node = other._node;
      _range = std::move(other._range);
      _name = std::move(other._name);
      UA_NodeId_init(&other._node); // Reset source
    }
    return *this;
  }

} // namespace ChimeraTK
