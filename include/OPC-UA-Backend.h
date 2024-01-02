// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
/*
 * OPC-UA-Backend.h
 *
 *  Created on: Nov 19, 2018
 *      Author: Klaus Zenker (HZDR)
 */
#include "OPC-UA-Connection.h"
#include "SubscriptionManager.h"
#include <unordered_set>

#include <ChimeraTK/BackendRegisterCatalogue.h>
#include <ChimeraTK/DeviceBackendImpl.h>

#include <boost/enable_shared_from_this.hpp>

#include <memory>
#include <mutex>
#include <sstream>

namespace ChimeraTK {
  class OPCUASubscriptionManager;
  /* -> Use UASet to make sure there are no duplicate nodes when adding nodes from the mapping file.
   * When browsing the server this should be avoided automatically.
  struct NodeComp{
  public:
    bool operator()(const UA_NodeId &lh, const UA_NodeId &rh) const{
      return UA_NodeId_equal(&lh,&rh);
    }
  };

  struct NodeHash{
  public:
    size_t operator()(const UA_NodeId& id) const{
      return UA_NodeId_hash(&id);
    }
  };

  typedef std::unordered_set<UA_NodeId, NodeHash, NodeComp> UASet;
  */

  /**
   *  RegisterInfo-derived class to be put into the RegisterCatalogue
   */
  class OpcUABackendRegisterInfo : public BackendRegisterInfoBase {
    //\ToDo: Adopt for OPC UA
   public:
    OpcUABackendRegisterInfo(const std::string& serverAddress, const std::string& node_browseName, const UA_NodeId& id)
    : _serverAddress(serverAddress), _nodeBrowseName(node_browseName), _id(id) {
      path = RegisterPath(serverAddress) / RegisterPath(node_browseName);
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
      dataDescriptor(other.dataDescriptor), _isReadonly(other._isReadonly), _arrayLength(other._arrayLength),
      _accessModes(other._accessModes) {
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
      _arrayLength = other._arrayLength;
      _accessModes = other._accessModes;
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
    size_t _arrayLength{0};
    AccessModeFlags _accessModes{};
    UA_NodeId _id;
  };

  /**
   * \remark Closing the an application using SIGINT will trigger closing the session. Thus, the state handler will
   * trigger OPCUASubscriptionManager::deactivateAllAndPushException. At the same time, in the destructor of the
   * Accessors the monitored items will be removed. This includes UA_Client_MonitoredItems_deleteSingle, which uses a
   * timeout. Since both methods use the subscription manager mutex shutting down the application takes some time.
   */
  class OpcUABackend : public DeviceBackendImpl {
   public:
    ~OpcUABackend();
    static boost::shared_ptr<DeviceBackend> createInstance(
        std::string address, std::map<std::string, std::string> parameters);

    /**
     * Callback triggered by state changes of the client connection.
     * Used here to detect when the client connection is set up.
     */
    static void stateCallback(UA_Client* client, UA_SecureChannelState channelState, UA_SessionState sessionState,
        UA_StatusCode recoveryStatus);

    /**
     * Callback triggered if inactivity of a subscription is detected.
     * This can happen if the server connection is cut and no communication is possible any more.
     */
    static void inactivityCallback(UA_Client* client, UA_UInt32 subId, void* subContext);

   protected:
    /**
     * \param fileAddress The address of the OPC UA server, e.g. opc.tcp://localhost:port.
     * \param username User name used when connecting to the OPC UA server.
     * \param password Password used when connecting to the OPC UA server.
     * \param mapfile The map file name.
     * \param subscriptonPublishingInterval Publishing interval used for the subscription in ms.
     * \param rootNode The root node specified.
     * \param rootNS The root node name space.
     * \param connectionTimeout
     */
    OpcUABackend(const std::string& fileAddress, const std::string& username = "", const std::string& password = "",
        const std::string& mapfile = "", const unsigned long& subscriptonPublishingInterval = 500,
        const std::string& rootNode = "", const ulong& rootNS = 0, const long int& connectionTimeout = 5000);

    /**
     * Fill catalog.
     *
     * If mapfile is empty the variable tree is parsed automatically by adding all nodes. This works only for ChimeraTK
     * server using the OPC UA ControlSystemAdapter. If in addition a _rootNode is given browsing will start at the root
     * node and all variables below are added. Else the root node of the OPC UA server is used.
     *
     * If a mapfile is given only node specified in the mapfile are considered.
     * Passing a _rootNode allows to prepend a certain hierarchy to the variables given in the map file.
     * E.g. if _rootNode is testServer and the variable in the map file is temperature/test fillCatalgogue will try to
     * add the node testServer/temperature/test.
     */
    void fillCatalogue();

    /**
     * Return the catalogue and if not filled yet fill it.
     */
    RegisterCatalogue getRegisterCatalogue() const override;

    void setExceptionImpl() noexcept override;

    /**
     * Catalogue is filled here.
     */
    void open() override;

    void close() override;

    std::string readDeviceInfo() override {
      std::stringstream ss;
      ss << "OPC-UA Server: " << _connection->serverAddress;
      return ss.str();
    }

    void activateAsyncRead() noexcept override;

    // Used to add the subscription support -> subscriptions are not active until activateAsyncRead() is called.
    void activateSubscriptionSupport();

    template<typename UserType>
    boost::shared_ptr<NDRegisterAccessor<UserType>> getRegisterAccessor_impl(
        const RegisterPath& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags);

    DEFINE_VIRTUAL_FUNCTION_TEMPLATE_VTABLE_FILLER(OpcUABackend, getRegisterAccessor_impl, 4);

    /** We need to make the catalogue mutable, since we fill it within getRegisterCatalogue() */
    mutable BackendRegisterCatalogue<OpcUABackendRegisterInfo> _catalogue_mutable;

    /** Class to register the backend type with the factory. */
    class BackendRegisterer {
     public:
      BackendRegisterer();
    };
    static BackendRegisterer backendRegisterer;
    static std::map<UA_Client*, OpcUABackend*> backendClients;

    template<typename UAType, typename CTKType>
    friend class OpcUABackendRegisterAccessor;

    bool isAsyncReadActive() {
      if(_subscriptionManager)
        return true;
      else
        return false;
    }

    std::shared_ptr<OPCUASubscriptionManager> _subscriptionManager;
    std::shared_ptr<OPCUAConnection> _connection;

   private:
    /**
     * Catalogue is filled when device is opened. When working with LogicalNameMapping the
     * catalogue is requested even if the device is not opened!
     * Keep track if catalogue is filled using this bool.
     */
    bool _catalogue_filled;

    bool _isFunctional{false};

    std::string _mapfile;

    std::string _rootNode;

    ulong _rootNS;

    /**
     * Protect against multiple calles of activateAsyncRead().
     *
     * This was observed for multiple LogicalNameMapping devices that reference the same device.
     * In the end OPCUASubscriptionManager::start() was called multiple times because the new thread was not yet created
     * when testing the thread in activateAsyncRead().
     */
    std::mutex _asyncReadLock;

    /**
     * Connect the client. If called after client is connected the connection is checked
     * and if it is ok no new connection is established.
     */
    void connect();

    /**
     * Reset subscription.
     */
    void resetClient();

    /**
     * Read the following node information:
     * - description
     * - data type
     *
     * Create a OpcUABackendRegisterInfo an set the dataDescriptor according to the data type.
     * Add the OpcUABackendRegisterInfo to the _catalogue_mutable.
     *
     * \param node The node to be added
     * \param nodeName An alternative node name. If not set the nodeName is set to the
     *        name of the node in case of a string node id and to "node_ID", where ID is
     *        the node id, in case of numeric node id.
     */
    void addCatalogueEntry(const UA_NodeId& node, std::shared_ptr<std::string> nodeName = nullptr);

    /**
     * Browse for nodes of type Variable.
     * If type Object is found move into the object and recall browseRecursive.
     */
    void browseRecursive(UA_NodeId startingNode = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER));

    /**
     * Read nodes from the file supplied as mapping file.
     * Expected file syntax:
     *  #Sting node id       Namespace id
     *  /dir/var1            1
     *  #Numeric node id     Namespace id
     *  123                  1
     *  #New Name   Sting node id      Namespace id
     *  myname1     /dir/var1          1
     *  #New name   Numeric node id    Namespace id
     *  myname2     123                1
     */
    void getNodesFromMapfile();
  };
} // namespace ChimeraTK
