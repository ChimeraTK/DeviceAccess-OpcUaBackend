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
        const std::string& description, const bool isReadonly);
  };

  /**
   *  RegisterInfo-derived class to be put into the RegisterCatalogue
   */
  class OpcUABackendRegisterInfo : public BackendRegisterInfoBase {
    //\ToDo: Adopt for OPC UA
   public:
    OpcUABackendRegisterInfo(const std::string& serverAddress, const std::string& node_browseName, const UA_NodeId& id)
    : _serverAddress(serverAddress), _nodeBrowseName(node_browseName) {
      path = RegisterPath(serverAddress) / RegisterPath(node_browseName);
      UA_NodeId_copy(&id, &_id);
      _namespaceIndex = _id.namespaceIndex;
      _isNumeric = _id.identifierType == UA_NodeIdType::UA_NODEIDTYPE_NUMERIC;
    }

    OpcUABackendRegisterInfo(const std::string& serverAddress, const std::string& node_browseName)
    : _serverAddress(serverAddress), _nodeBrowseName(node_browseName) {
      path = RegisterPath(serverAddress) / RegisterPath(node_browseName);
    }

    OpcUABackendRegisterInfo() = default;

    ~OpcUABackendRegisterInfo() override { UA_NodeId_clear(&_id); }

    OpcUABackendRegisterInfo(const OpcUABackendRegisterInfo& other)
    : path(other.path), _serverAddress(other._serverAddress), _nodeBrowseName(other._nodeBrowseName),
      _description(other._description), _unit(other._unit), _dataType(other._dataType),
      dataDescriptor(other.dataDescriptor), _isReadonly(other._isReadonly), _isNumeric(other._isNumeric),
      _arrayLength(other._arrayLength), _accessModes(other._accessModes), _indexRange(other._indexRange),
      _namespaceIndex(other._namespaceIndex) {
      UA_NodeId_copy(&other._id, &_id);
    }

    OpcUABackendRegisterInfo& operator=(const OpcUABackendRegisterInfo& other) {
      path = other.path;
      _serverAddress = other._serverAddress;
      _nodeBrowseName = other._nodeBrowseName;
      _description = other._description;
      _unit = other._unit;
      _dataType = other._dataType;
      dataDescriptor = other.dataDescriptor;
      _isReadonly = other._isReadonly;
      _isNumeric = other._isNumeric; //?< Needed for caching
      _arrayLength = other._arrayLength;
      _accessModes = other._accessModes;
      _indexRange = other._indexRange;
      _namespaceIndex = other._namespaceIndex; //?< Needed for caching
      UA_NodeId_copy(&other._id, &_id);
      return *this;
    }

    RegisterPath getRegisterName() const override { return RegisterPath(_nodeBrowseName); }

    std::string getRegisterPath() const { return path; }

    unsigned int getNumberOfElements() const override { return _arrayLength; }

    unsigned int getNumberOfChannels() const override { return 1; }

    const DataDescriptor& getDataDescriptor() const override { return dataDescriptor; }

    bool isReadable() const override { return true; }

    bool isWriteable() const override { return !_isReadonly; }

    AccessModeFlags getSupportedAccessModes() const override { return _accessModes; }

    std::unique_ptr<BackendRegisterInfoBase> clone() const override {
      return std::unique_ptr<BackendRegisterInfoBase>(new OpcUABackendRegisterInfo(*this));
    }

    RegisterPath path;
    std::string _serverAddress;
    std::string _nodeBrowseName;
    std::string _description;
    std::string _unit;
    UA_UInt32 _dataType{0};
    DataDescriptor dataDescriptor;
    bool _isReadonly{true};
    bool _isNumeric{true};
    uint16_t _namespaceIndex{0};
    size_t _arrayLength{0};
    AccessModeFlags _accessModes{};
    UA_NodeId _id;
    std::string _indexRange{""};
  };
} // namespace ChimeraTK
