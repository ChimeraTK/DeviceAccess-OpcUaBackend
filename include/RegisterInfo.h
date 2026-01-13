// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <ChimeraTK/BackendRegisterCatalogue.h>

#include <open62541/client_highlevel.h>
/*
 * RegisterInfo.h
 *
 *  Created on: Sep 18, 2025
 *      Author: Klaus Zenker (HZDR)
 */
namespace ChimeraTK {
  class OpcUABackendRegisterInfo;
  class OpcUaBackendRegisterCatalogue : public ChimeraTK::BackendRegisterCatalogue<OpcUABackendRegisterInfo> {
   public:
    // Add all registers for the given property. Registers which exist already in the catalogue (with the same name) are
    // skipped.
    void addProperty(const UA_NodeId& node, const std::string& browseName, const std::string& range,
        const UA_UInt32& dataType, const size_t& arrayLength, const std::string& serverAddress,
        const std::string& description, const bool& isReadonly);
  };

  /**
   *  RegisterInfo-derived class to be put into the RegisterCatalogue
   */
  class OpcUABackendRegisterInfo : public BackendRegisterInfoBase {
    //\ToDo: Adopt for OPC UA
   public:
    OpcUABackendRegisterInfo(
        const std::string& serverAddress, const std::string& node_browseName, const UA_NodeId& idPassed)
    : serverAddress(serverAddress), nodeBrowseName(node_browseName) {
      path = RegisterPath(serverAddress) / RegisterPath(node_browseName);
      UA_NodeId_init(&id);
      UA_NodeId_copy(&idPassed, &id);
      namespaceIndex = id.namespaceIndex;
      isNumeric = id.identifierType == UA_NodeIdType::UA_NODEIDTYPE_NUMERIC;
    }

    OpcUABackendRegisterInfo(const std::string& serverAddress, const std::string& node_browseName)
    : serverAddress(serverAddress), nodeBrowseName(node_browseName) {
      path = RegisterPath(serverAddress) / RegisterPath(node_browseName);
      UA_NodeId_init(&id);
    }

    OpcUABackendRegisterInfo() = default;

    ~OpcUABackendRegisterInfo() override { UA_NodeId_clear(&id); }

    OpcUABackendRegisterInfo(const OpcUABackendRegisterInfo& other)
    : path(other.path), serverAddress(other.serverAddress), nodeBrowseName(other.nodeBrowseName),
      description(other.description), unit(other.unit), dataType(other.dataType), dataDescriptor(other.dataDescriptor),
      isReadonly(other.isReadonly), isNumeric(other.isNumeric), arrayLength(other.arrayLength),
      accessModes(other.accessModes), indexRange(other.indexRange), namespaceIndex(other.namespaceIndex) {
      UA_NodeId_init(&id);
      UA_NodeId_copy(&other.id, &id);
    }

    OpcUABackendRegisterInfo& operator=(const OpcUABackendRegisterInfo& other) {
      path = other.path;
      serverAddress = other.serverAddress;
      nodeBrowseName = other.nodeBrowseName;
      description = other.description;
      unit = other.unit;
      dataType = other.dataType;
      dataDescriptor = other.dataDescriptor;
      isReadonly = other.isReadonly;
      isNumeric = other.isNumeric; //?< Needed for caching
      arrayLength = other.arrayLength;
      accessModes = other.accessModes;
      indexRange = other.indexRange;
      namespaceIndex = other.namespaceIndex; //?< Needed for caching
      UA_NodeId_copy(&other.id, &id);
      return *this;
    }

    RegisterPath getRegisterName() const override { return RegisterPath(nodeBrowseName); }

    std::string getRegisterPath() const { return path; }

    unsigned int getNumberOfElements() const override { return arrayLength; }

    unsigned int getNumberOfChannels() const override { return 1; }

    const DataDescriptor& getDataDescriptor() const override { return dataDescriptor; }

    bool isReadable() const override { return true; }

    bool isWriteable() const override { return !isReadonly; }

    AccessModeFlags getSupportedAccessModes() const override { return accessModes; }

    std::unique_ptr<BackendRegisterInfoBase> clone() const override {
      return std::unique_ptr<BackendRegisterInfoBase>(new OpcUABackendRegisterInfo(*this));
    }

    RegisterPath path;
    std::string serverAddress;
    std::string nodeBrowseName;
    std::string description;
    std::string unit;
    UA_UInt32 dataType{0};
    DataDescriptor dataDescriptor;
    bool isReadonly{true};
    bool isNumeric{true};
    uint16_t namespaceIndex{0};
    size_t arrayLength{0};
    AccessModeFlags accessModes{};
    UA_NodeId id{};
    std::string indexRange{""};
  };
} // namespace ChimeraTK
