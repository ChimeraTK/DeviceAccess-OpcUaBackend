// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

/*
 * MapFile.h
 *
 *  Created on: Aug 7, 2024
 *      Author: Klaus Zenker (HZDR)
 */
#include "OPC-UA-Backend.h"

#include <libxml++/libxml++.h>
#include <open62541/types.h>

#include <deque>
#include <string>

namespace ChimeraTK {
  struct MapElement {
    UA_UInt16 _namespace;     ///< Namespace
    std::string _strNode{""}; ///< String node ID
    UA_UInt32 _iNode{0};
    UA_NodeId _node;        ///< OPC UA Node Id
    std::string _range{""}; ///< Range string, e.g. 2:4
    std::string _name{""};  ///< Name that is used in the device. If empty it is constructed from the nodeID
    /**
     * Construct MapElement for int node ID.
     * @param id Node ID.
     * @param ns Namespace.
     * @param range Range string, e.g. 3:8.
     * @param name Name that is used in the device. If empty it is constructed from the nodeID
     */
    MapElement(const UA_UInt32& id, const UA_UInt16& ns, const std::string& range, const std::string& name);
    /**
     * Construct MapElement for string node ID.
     * @param id Node ID.
     * @param ns Namespace.
     * @param range Range string, e.g. 3:8.
     * @param name Name that is used in the device. If empty it is constructed from the nodeID
     */
    MapElement(const std::string& id, const UA_UInt16& ns, const std::string& range, const std::string& name);
  };

  struct OPCUAMapFileReader {
    /** @brief The constructor of the class creates a doc pointer depending on the file path
     *
     * @param filePath Path to a xml file which you want to read
     * @param rootNode Root node name to be prepended to all nodes. Might left empty.
     */
    explicit OPCUAMapFileReader(const std::string& filePath, const std::string& rootNode = "");
    std::deque<MapElement> elements; ///< Contains all elements listed in the map file.
   private:
    /** @brief Parse the map file and fill the element list.
     */
    void readElements();
    xmlpp::Element* _rootNode{nullptr};
    std::string _file; ///< Name of the map file
    std::string _serverRootNode;
  };
} // namespace ChimeraTK
