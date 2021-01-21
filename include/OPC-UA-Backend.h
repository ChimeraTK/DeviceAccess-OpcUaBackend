/*
 * OPC-UA-Backend.h
 *
 *  Created on: Nov 19, 2018
 *      Author: zenker
 */

#ifndef OPC_UA_BACKEND_H_
#define OPC_UA_BACKEND_H_

#include <ChimeraTK/DeviceBackendImpl.h>

#include <boost/enable_shared_from_this.hpp>

#include "open62541.h"
#include <sstream>
#include <mutex>
#include <memory>
#include <unordered_set>

namespace ChimeraTK {

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
  class OpcUABackendRegisterInfo : public RegisterInfo {
    //\ToDo: Adopt for OPC UA
    public:
      OpcUABackendRegisterInfo(const std::string &serverAddress, const std::string &node_browseName, const UA_NodeId &id):
      _serverAddress(serverAddress), _nodeBrowseName(node_browseName), _id(id){
        path = RegisterPath(serverAddress)/RegisterPath(node_browseName);
      }

      OpcUABackendRegisterInfo(const std::string &serverAddress, const std::string &node_browseName):
      _serverAddress(serverAddress), _nodeBrowseName(node_browseName){
        path = RegisterPath(serverAddress)/RegisterPath(node_browseName);
      }

      virtual ~OpcUABackendRegisterInfo() {}

      RegisterPath getRegisterName() const override { return RegisterPath(_nodeBrowseName); }

      std::string getRegisterPath() const { return path; }

      unsigned int getNumberOfElements() const override { return _arrayLength; }

      unsigned int getNumberOfChannels() const override { return 1; }

      unsigned int getNumberOfDimensions() const override { return _arrayLength > 1 ? 1 : 0; }

      const RegisterInfo::DataDescriptor& getDataDescriptor() const override { return dataDescriptor; }

      bool isReadable() const override {return true;}

      bool isWriteable() const override {return !_isReadonly;}

      AccessModeFlags getSupportedAccessModes() const override {return _accessModes;}

      RegisterPath path;
      std::string _serverAddress;
      std::string _nodeBrowseName;
      std::string _description;
      std::string _unit;
      UA_UInt32 _dataType;
      RegisterInfo::DataDescriptor dataDescriptor;
      bool _isReadonly;
      size_t _arrayLength;
      AccessModeFlags _accessModes{};
      UA_NodeId _id;

  };

  class OpcUABackend : public DeviceBackendImpl{
  public:
    /**
     * This is used since async access is not supported by OPC-UA.
     [12/26/2018 20:55:49.705] error/client Reply answers the wrong requestId. Async services are not yet implemented.
     [12/26/2018 20:55:49.705] info/client  Error receiving the response
     * One could also use a client per process variable...
     */
    std::mutex opcua_mutex;
    ~OpcUABackend();
    static boost::shared_ptr<DeviceBackend> createInstance(std::string address, std::map<std::string,std::string> parameters);
  protected:
    OpcUABackend(const std::string &fileAddress, const unsigned long &port, const std::string &username = "", const std::string &password = "", const std::string &mapfile = "", const unsigned long &subscriptonPublishingInterval = 500);

    void fillCatalogue();

    /**
     * Return the catalogue and if not filled yet fill it.
     */
    const RegisterCatalogue& getRegisterCatalogue() const override;

    void setException() override;

    /**
     * Catalogue is filled here.
     */
    void open() override;

    void close() override;

    std::string readDeviceInfo() override {
      std::stringstream ss;
      ss << "OPC-UA Server: " << _serverAddress << ":" << _port;
      return ss.str();
    }

    bool isFunctional() const override;

    void activateAsyncRead() noexcept override;

    template<typename UserType>
    boost::shared_ptr< NDRegisterAccessor<UserType> > getRegisterAccessor_impl(const RegisterPath &registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags);

    DEFINE_VIRTUAL_FUNCTION_TEMPLATE_VTABLE_FILLER( OpcUABackend, getRegisterAccessor_impl, 4);

    /** We need to make the catalogue mutable, since we fill it within getRegisterCatalogue() */
    mutable RegisterCatalogue _catalogue_mutable;

    /** Class to register the backend type with the factory. */
    class BackendRegisterer {
      public:
        BackendRegisterer();
    };
    static BackendRegisterer backendRegisterer;


    template<typename UAType, typename CTKType>
    friend class OpcUABackendRegisterAccessor;
		
		// This needs to be public because it is accessed by the RegisterAccessor.
    UA_Client *_client;

    bool isAsyncReadActive(){return _asyncReadActivated;}

  private:
    /**
     * Catalogue is filled when device is opened. When working with LogicalNameMapping the
     * catalogue is requested even if the device is not opened!
     * Keep track if catalogue is filled using this bool.
     */
    bool _catalogue_filled;

    bool _isFunctional{false};

    bool _asyncReadActivated{false};

    std::string _serverAddress;

    unsigned long _port;

    std::string _username;
    std::string _password;
    std::string _mapfile;

    UA_ClientConfig _config;

    uint32_t _publishingInterval;

    /**
     * Connect the client. If called after client is connected the connection is checked
     * and if it is ok no new connection is established.
     * \param initialCall True if called in the constructor.
     */
    void connect(bool initalCall = false);

    /**
     * Delete the client connection and set the client pointer
     * to nullptr
     */
    void deleteClient();

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
    void addCatalogueEntry(const UA_NodeId &node, std::shared_ptr<std::string> nodeName = nullptr);

    /**
     * Browse for nodes of type Variable.
     * If type Object is found move into the object and recall browseRecursive.
     */
    void browseRecursive(UA_Client *client, UA_NodeId startingNode = UA_NODEID_NUMERIC(0,UA_NS0ID_OBJECTSFOLDER));

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

    UA_ClientState getConnectionState();

  };
}



#endif /* OPC_UA_BACKEND_H_ */
