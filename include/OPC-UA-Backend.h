/*
 * OPC-UA-Backend.h
 *
 *  Created on: Nov 19, 2018
 *      Author: zenker
 */

#ifndef OPC_UA_BACKEND_H_
#define OPC_UA_BACKEND_H_

#include <ChimeraTK/DeviceBackendImpl.h>

#include "open62541.h"
#include <sstream>

namespace ChimeraTK {
  class OpcUABackend : public DeviceBackendImpl {
  public:
    virtual ~OpcUABackend(){}
  protected:
    OpcUABackend(const std::string &fileAddress, const unsigned long &port, const std::string &username = "", const std::string &password = "");

    void fillCatalogue() const;

    /**
     * Return the catalogue and if not filled yet fill it.
     */
    const RegisterCatalogue& getRegisterCatalogue() const override;

    /**
     * Catalogue is filled here.
     */
    void open() override;

    void close() override;

    std::string readDeviceInfo() override {
      std::stringstream ss;
      ss << "OPC-UA Server: ";
      return ss.str();
    }

    static boost::shared_ptr<DeviceBackend> createInstance(std::string address, std::map<std::string,std::string> parameters);

    template<typename UserType>
    boost::shared_ptr< NDRegisterAccessor<UserType> > getRegisterAccessor_impl(
        const RegisterPath &registerPathName);
    DEFINE_VIRTUAL_FUNCTION_TEMPLATE_VTABLE_FILLER( OpcUABackend, getRegisterAccessor_impl, 1);

    /** We need to make the catalogue mutable, since we fill it within getRegisterCatalogue() */
    mutable RegisterCatalogue _catalogue_mutable;

    /** Class to register the backend type with the factory. */
    class BackendRegisterer {
      public:
        BackendRegisterer();
    };
    static BackendRegisterer backendRegisterer;


    template<typename UserType>
    friend class OpcUABackendRegisterAccessor;

  private:
    /**
     * Catalogue is filled when device is opened. When working with LogicalNameMapping the
     * catalogue is requested even if the device is not opened!
     * Keep track if catalogue is filled using this bool.
     */
    bool _catalogue_filled;

    std::string _serverAddress;

    unsigned long _port;

    std::string _username;
    std::string _password;

    UA_Client *_client;

    /**
     * Delete the client connection and set the client pointer
     * to nullptr
     */
    void deleteClient();

    /**
     * Used to iteratively loop over all device files
     */
    void addCatalogueEntry(const UA_UInt32 &node) const;

    /**
     * Browse all references of the given node and return a list of nodes from namespace 1 that are numeric nodes.
     */
    std::set<UA_UInt32> browse(UA_UInt32 node, UA_UInt16 ns = 1) const;
    /**
     * Get all nodes that include a "/" in the browse name.
     */
    std::set<UA_UInt32> findServerNodes(UA_UInt32 node) const;

  };
}



#endif /* OPC_UA_BACKEND_H_ */
