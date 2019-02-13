/*
 * OPC-UA-BackendRegisterAccessor.h
 *
 *  Created on: Nov 19, 2018
 *      Author: zenker
 */

#ifndef OPC_UA_BACKENDREGISTERACCESSOR_H_
#define OPC_UA_BACKENDREGISTERACCESSOR_H_

#include <ChimeraTK/SyncNDRegisterAccessor.h>
#include <ChimeraTK/RegisterPath.h>
#include <ChimeraTK/AccessMode.h>

#include <sstream>

namespace ChimeraTK {
  std::mutex opcua_mutex;
template<typename UserType>
  class OpcUABackendRegisterAccessor : public SyncNDRegisterAccessor<UserType> {

  public:

   virtual ~OpcUABackendRegisterAccessor(){this->shutdown();};

   virtual void doReadTransfer() ;

   bool doReadTransferNonBlocking() override;

   bool doReadTransferLatest() override;

   virtual bool doWriteTransfer(ChimeraTK::VersionNumber /*versionNumber*/={}) override;

   AccessModeFlags getAccessModeFlags() const override {
     return {};
   }

  protected:

   OpcUABackendRegisterAccessor(const RegisterPath &path, UA_Client *client,const std::string &node_id, const bool &isReadOnly, OpcUABackend* backend);

   bool isReadOnly() const override {
     return _isReadOnly;
   }

   bool isReadable() const override {
     return true;
   }

   bool isWriteable() const override {
     return !_isReadOnly;
   }

   std::vector< boost::shared_ptr<TransferElement> > getHardwareAccessingElements() override {
     return { boost::enable_shared_from_this<TransferElement>::shared_from_this() };
   }

   std::list<boost::shared_ptr<TransferElement> > getInternalElements() override {
     return {};
   }

   void replaceTransferElement(boost::shared_ptr<TransferElement> /*newElement*/) override {} // LCOV_EXCL_LINE

   friend class OpcUABackend;

   UA_Client *_client;
   std::string _node_id;
   bool _isReadOnly;
   size_t _arraySize;
   bool _isScalar;
   OpcUABackend* _backend;

  };

  template<typename UserType>
  OpcUABackendRegisterAccessor<UserType>::OpcUABackendRegisterAccessor(const RegisterPath &path, UA_Client *client, const std::string &node_id, const bool &isReadOnly, OpcUABackend *backend)
  : SyncNDRegisterAccessor<UserType>(path), _client(client), _node_id(node_id), _isReadOnly(isReadOnly), _backend(backend)
  {
    std::lock_guard<std::mutex> lock(opcua_mutex);
    //\ToDo: Check if variable is array
    try {
      UA_Variant *val = UA_Variant_new();
      UA_StatusCode retval = UA_Client_readValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);

      if(retval != UA_STATUSCODE_GOOD){
        UA_Variant_delete(val);
        std::stringstream out;
        out << "OPC-UA-Backend::Failed to read data from variable: " << _node_id << " with reason: " << std::hex << retval;
        throw ChimeraTK::runtime_error(out.str());
      }
      // allocate buffers
      if(UA_Variant_isScalar(val)){
        NDRegisterAccessor<UserType>::buffer_2D.resize(1);
        NDRegisterAccessor<UserType>::buffer_2D[0].resize(1);
        _arraySize = 1;
        _isScalar = true;
      } else {
        _isScalar = false;
        NDRegisterAccessor<UserType>::buffer_2D.resize(1);
        if(val->arrayLength == 0){
          throw ChimeraTK::runtime_error("Array length is 0!");
        } else {
          NDRegisterAccessor<UserType>::buffer_2D[0].resize(val->arrayLength);
          _arraySize = val->arrayLength;
        }
      }

    }
    catch(ChimeraTK::runtime_error &e){
      std::cerr << e.what() << std::endl;
      this->shutdown();
      throw ChimeraTK::runtime_error("OPC-UA-Backend::Failed to setup the accessor.");;
    } catch(...) {
      std::cerr << "OPC-UA-Backend::Shuting down..." << std::endl;
      this->shutdown();
      throw ChimeraTK::runtime_error("OPC-UA-Backend::Failed to setup the accessor.");;
    }
  }


  template<typename UserType>
  void OpcUABackendRegisterAccessor<UserType>::doReadTransfer() {
    throw ChimeraTK::runtime_error("Data type not supported by OpcUABackendRegisterAccessor.");
  }

  template<typename UserType>
  bool OpcUABackendRegisterAccessor<UserType>::doReadTransferLatest() {
    doReadTransfer();
    return true;
  }

  template<typename UserType>
  bool OpcUABackendRegisterAccessor<UserType>::doReadTransferNonBlocking() {
    doReadTransfer();
    return true;
  }

  template<typename UserType>
  bool OpcUABackendRegisterAccessor<UserType>::doWriteTransfer(ChimeraTK::VersionNumber){
    throw ChimeraTK::runtime_error("Data type not supported by OpcUABackendRegisterAccessor.");
  }

  template<>
  void OpcUABackendRegisterAccessor<int32_t>::doReadTransfer() {
    std::lock_guard<std::mutex> lock(opcua_mutex);
//    std::cout << "Reading int value of node (1, \"" << _node_id << "\":" << std::endl;
    UA_Variant *val = UA_Variant_new();
    UA_StatusCode retval = UA_Client_readValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);

    if(retval == UA_STATUSCODE_BADCONNECTIONCLOSED){
      //UA_Client_disconnect(_client);
//      UA_Client_manuallyRenewSecureChannel(_client)
      std::cerr << "OPC-UA-Backend::Resetting client..." << std::endl;
      _backend->reconnect();
      retval = UA_Client_readValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);
    }

    if(retval == UA_STATUSCODE_GOODNONCRITICALTIMEOUT){
      //UA_Client_disconnect(_client);
//      UA_Client_manuallyRenewSecureChannel(_client)
      std::cerr << "OPC-UA-Backend::Resetting client because of timeout..." << std::endl;
      _backend->reconnect();
      retval = UA_Client_readValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);
    }
    if(retval != UA_STATUSCODE_GOOD){
      UA_Variant_delete(val);
      std::stringstream out;
      out << "OPC-UA-Backend::Failed to read data from variable: " << _node_id << " with reason: " << std::hex << retval;
      throw ChimeraTK::runtime_error(out.str());
    }
    if(val->type == &UA_TYPES[UA_TYPES_INT32]) {
      for(size_t i = 0; i < _arraySize; i++){
        UA_Int32* tmp = (UA_Int32*)val->data;
        UA_Int32 value = tmp[i];
        NDRegisterAccessor<int32_t>::buffer_2D[0][i] = value;
//        printf("the value is: %i\n", value);
      }
      UA_Variant_delete(val);
    } else if (val->type == &UA_TYPES[UA_TYPES_UINT32]) {
      for(size_t i = 0; i < _arraySize; i++){
        UA_UInt32* tmp = (UA_UInt32*)val->data;
        UA_UInt32 value = tmp[i];
        NDRegisterAccessor<int32_t>::buffer_2D[0][i] = value;
//        printf("the value is: %i\n", value);
      }
      UA_Variant_delete(val);
    } else if (val->type == &UA_TYPES[UA_TYPES_FLOAT] ||
               val->type == &UA_TYPES[UA_TYPES_DOUBLE]){
      UA_Variant_delete(val);
      throw ChimeraTK::runtime_error(std::string("OPC-UA-Backend::The variable: ") + _node_id + " is not of an integer like type. "
          "It will not be casted! Consider using the correct accessor.");
    } else {
      UA_Variant_delete(val);
      throw ChimeraTK::runtime_error(std::string("OPC-UA-Backend::The variable: ") + _node_id + " has an unsupported data type: " + val->type->typeName);
    }
  }

  template<>
  bool OpcUABackendRegisterAccessor<int32_t>::doReadTransferLatest() {
    doReadTransfer();
    return true;
  }

  template<>
  bool OpcUABackendRegisterAccessor<int32_t>::doReadTransferNonBlocking() {
    doReadTransfer();
    return true;
  }


  template<>
  bool OpcUABackendRegisterAccessor<int32_t>::doWriteTransfer(ChimeraTK::VersionNumber){
    std::lock_guard<std::mutex> lock(opcua_mutex);
    UA_Variant *val = UA_Variant_new();
    if(_isScalar){
      UA_Variant_setScalarCopy(val, &NDRegisterAccessor<int32_t>::buffer_2D[0][0], &UA_TYPES[UA_TYPES_INT32]);
    } else {
      UA_Variant_setArrayCopy(val, &NDRegisterAccessor<int32_t>::buffer_2D[0][0], _arraySize,  &UA_TYPES[UA_TYPES_INT32]);
    }
    UA_StatusCode retval = UA_Client_writeValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);
    UA_Variant_delete(val);
    if(retval == UA_STATUSCODE_GOOD){
      return true;
    } else if (retval == UA_STATUSCODE_BADNOTWRITABLE || retval == UA_STATUSCODE_BADWRITENOTSUPPORTED){
      throw ChimeraTK::runtime_error(std::string("OPC-UA-Backend::Variable ") + _node_id + " is not writable!");
    } else {
//      printf("Failed writing with error code: %zu", retval);
      return false;
    }
  }

  template<>
  void OpcUABackendRegisterAccessor<uint>::doReadTransfer() {
    std::lock_guard<std::mutex> lock(opcua_mutex);
    UA_Variant *val = UA_Variant_new();
    UA_StatusCode retval = UA_Client_readValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);
    if(retval != UA_STATUSCODE_GOOD){
      UA_Variant_delete(val);
      std::stringstream out;
      out << "OPC-UA-Backend::Failed to read data from variable: " << _node_id << " with reason: " << std::hex << retval;
      throw ChimeraTK::runtime_error(out.str());
    }
    if(val->type == &UA_TYPES[UA_TYPES_UINT32]) {
      for(size_t i = 0; i < _arraySize; i++){
        UA_UInt32* tmp = (UA_UInt32*)val->data;
        UA_UInt32 value = tmp[i];
        NDRegisterAccessor<uint>::buffer_2D[0][i] = value;
      }
      UA_Variant_delete(val);
    } else if (val->type == &UA_TYPES[UA_TYPES_INT32] ||
               val->type == &UA_TYPES[UA_TYPES_FLOAT] ||
               val->type == &UA_TYPES[UA_TYPES_DOUBLE]){
      UA_Variant_delete(val);
      throw ChimeraTK::runtime_error(std::string("OPC-UA-Backend::The variable: ") + _node_id + " is not of an unsigned integer like type. "
          "It will not be casted! Consider using the correct accessor.");
    } else {
      UA_Variant_delete(val);
      throw ChimeraTK::runtime_error(std::string("OPC-UA-Backend::Type mismatch when reading uint variable: ") + _node_id);
    }
  }

  template<>
  bool OpcUABackendRegisterAccessor<uint>::doReadTransferLatest() {
    doReadTransfer();
    return true;
  }

  template<>
  bool OpcUABackendRegisterAccessor<uint>::doReadTransferNonBlocking() {
    doReadTransfer();
    return true;
  }

  template<>
  bool OpcUABackendRegisterAccessor<uint>::doWriteTransfer(ChimeraTK::VersionNumber){
    std::lock_guard<std::mutex> lock(opcua_mutex);
    UA_Variant *val = UA_Variant_new();
    if(_isScalar){
      UA_Variant_setScalarCopy(val, &NDRegisterAccessor<uint>::buffer_2D[0][0], &UA_TYPES[UA_TYPES_UINT32]);
    } else {
      UA_Variant_setArrayCopy(val, &NDRegisterAccessor<uint>::buffer_2D[0][0], _arraySize,  &UA_TYPES[UA_TYPES_UINT32]);
    }
    UA_StatusCode retval = UA_Client_writeValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);
    UA_Variant_delete(val);
    if(retval == UA_STATUSCODE_GOOD){
      return true;
    } else if (retval == UA_STATUSCODE_BADNOTWRITABLE || retval == UA_STATUSCODE_BADWRITENOTSUPPORTED){
      throw ChimeraTK::runtime_error(std::string("OPC-UA-Backend::Variable ") + _node_id + " is not writable!");
    } else {
//      printf("Failed writing with error code: %zu", retval);
      return false;
    }
  }

  template<>
  void OpcUABackendRegisterAccessor<std::string>::doReadTransfer() {
    std::lock_guard<std::mutex> lock(opcua_mutex);
    UA_Variant *val = UA_Variant_new();
    UA_StatusCode retval = UA_Client_readValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);
    if(retval == UA_STATUSCODE_GOOD && val->type == &UA_TYPES[UA_TYPES_STRING]) {
      for(size_t i = 0; i < _arraySize; i++){
        UA_String* tmp = (UA_String*)val->data;
        UA_String value = tmp[i];
        if(value.length != 0)
          NDRegisterAccessor<std::string>::buffer_2D[0][i] = std::string((char*)value.data, value.length);
        else
          NDRegisterAccessor<std::string>::buffer_2D[0][i] = std::string("");
      }
    }
    UA_Variant_delete(val);
  }

  template<>
  bool OpcUABackendRegisterAccessor<std::string>::doReadTransferLatest() {
    doReadTransfer();
    return true;
  }

  template<>
  bool OpcUABackendRegisterAccessor<std::string>::doReadTransferNonBlocking() {
    doReadTransfer();
    return true;
  }

  template<>
  bool OpcUABackendRegisterAccessor<std::string>::doWriteTransfer(ChimeraTK::VersionNumber){
    std::lock_guard<std::mutex> lock(opcua_mutex);
    UA_Variant *val = UA_Variant_new();
    if(_isScalar){
      UA_Variant_setScalarCopy(val, &NDRegisterAccessor<std::string>::buffer_2D[0][0], &UA_TYPES[UA_TYPES_STRING]);
    } else {
      UA_Variant_setArrayCopy(val, &NDRegisterAccessor<std::string>::buffer_2D[0][0], _arraySize,  &UA_TYPES[UA_TYPES_STRING]);
    }
    UA_StatusCode retval = UA_Client_writeValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);
    UA_Variant_delete(val);
    if(retval == UA_STATUSCODE_GOOD){
      return true;
    } else if (retval == UA_STATUSCODE_BADNOTWRITABLE || retval == UA_STATUSCODE_BADWRITENOTSUPPORTED){
      throw ChimeraTK::runtime_error(std::string("OPC-UA-Backend::Variable ") + _node_id + " is not writable!");
    } else {
//      printf("Failed writing with error code: %zu", retval);
      return false;
    }
  }

  template<>
  void OpcUABackendRegisterAccessor<double>::doReadTransfer() {
    std::lock_guard<std::mutex> lock(opcua_mutex);
    UA_Variant *val = UA_Variant_new();
    UA_StatusCode retval = UA_Client_readValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);
    if(retval != UA_STATUSCODE_GOOD){
      UA_Variant_delete(val);
      std::stringstream out;
      out << "OPC-UA-Backend::Failed to read data from variable: " << _node_id << " with reason: " << std::hex << retval;
      throw ChimeraTK::runtime_error(out.str());
    }
    if(val->type == &UA_TYPES[UA_TYPES_DOUBLE]) {
      for(size_t i = 0; i < _arraySize; i++){
        UA_Double* tmp = (UA_Double*)val->data;
        UA_Double value = tmp[i];
        NDRegisterAccessor<double>::buffer_2D[0][i] = value;
      }
      UA_Variant_delete(val);
    } else if (val->type == &UA_TYPES[UA_TYPES_UINT32]){
      for(size_t i = 0; i < _arraySize; i++){
        UA_UInt32* tmp = (UA_UInt32*)val->data;
        UA_UInt32 value = tmp[i];
        NDRegisterAccessor<double>::buffer_2D[0][i] = value;
      }
      UA_Variant_delete(val);
    } else if (val->type == &UA_TYPES[UA_TYPES_INT32]){
      for(size_t i = 0; i < _arraySize; i++){
        UA_Int32* tmp = (UA_Int32*)val->data;
        UA_Int32 value = tmp[i];
        NDRegisterAccessor<double>::buffer_2D[0][i] = value;
      }
      UA_Variant_delete(val);
    } else if (val->type == &UA_TYPES[UA_TYPES_FLOAT]){
      for(size_t i = 0; i < _arraySize; i++){
        UA_Float* tmp = (UA_Float*)val->data;
        UA_Float value = tmp[i];
        NDRegisterAccessor<double>::buffer_2D[0][i] = value;
      }
      UA_Variant_delete(val);
    } else {
      UA_Variant_delete(val);
      throw ChimeraTK::runtime_error(std::string("OPC-UA-Backend::Type mismatch when reading double variable: ") + _node_id);
    }
  }

  template<>
  bool OpcUABackendRegisterAccessor<double>::doReadTransferLatest() {
    doReadTransfer();
    return true;
  }

  template<>
  bool OpcUABackendRegisterAccessor<double>::doReadTransferNonBlocking() {
    doReadTransfer();
    return true;
  }

  template<>
  bool OpcUABackendRegisterAccessor<double>::doWriteTransfer(ChimeraTK::VersionNumber){
    std::lock_guard<std::mutex> lock(opcua_mutex);
    UA_Variant *val = UA_Variant_new();
    if(_isScalar){
      UA_Variant_setScalarCopy(val, &NDRegisterAccessor<double>::buffer_2D[0][0], &UA_TYPES[UA_TYPES_DOUBLE]);
    } else {
      UA_Variant_setArrayCopy(val, &NDRegisterAccessor<double>::buffer_2D[0][0], _arraySize,  &UA_TYPES[UA_TYPES_DOUBLE]);
    }
    UA_StatusCode retval = UA_Client_writeValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);
    UA_Variant_delete(val);
    if(retval == UA_STATUSCODE_GOOD){
      return true;
    } else if (retval == UA_STATUSCODE_BADNOTWRITABLE || retval == UA_STATUSCODE_BADWRITENOTSUPPORTED){
      throw ChimeraTK::runtime_error(std::string("OPC-UA-Backend::Variable ") + _node_id + " is not writable!");
    } else {
//      printf("Failed writing with error code: %zu", retval);
      return false;
    }
  }

  template<>
  void OpcUABackendRegisterAccessor<float>::doReadTransfer() {
    std::lock_guard<std::mutex> lock(opcua_mutex);
    UA_Variant *val = UA_Variant_new();
    UA_StatusCode retval = UA_Client_readValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);
    if(retval != UA_STATUSCODE_GOOD){
      UA_Variant_delete(val);
      std::stringstream out;
      out << "OPC-UA-Backend::Failed to read data from variable: " << _node_id << " with reason: " << std::hex << retval;
      throw ChimeraTK::runtime_error(out.str());
    }
    if(val->type == &UA_TYPES[UA_TYPES_FLOAT]) {
      for(size_t i = 0; i < _arraySize; i++){
        UA_Float* tmp = (UA_Float*)val->data;
        UA_Float value = tmp[i];
        NDRegisterAccessor<float>::buffer_2D[0][i] = value;
      }
      UA_Variant_delete(val);
    } else if (val->type == &UA_TYPES[UA_TYPES_UINT32]){
      for(size_t i = 0; i < _arraySize; i++){
        UA_UInt32* tmp = (UA_UInt32*)val->data;
        UA_UInt32 value = tmp[i];
        NDRegisterAccessor<double>::buffer_2D[0][i] = value;
      }
      UA_Variant_delete(val);
    } else if (val->type == &UA_TYPES[UA_TYPES_INT32]){
      for(size_t i = 0; i < _arraySize; i++){
        UA_Int32* tmp = (UA_Int32*)val->data;
        UA_Int32 value = tmp[i];
        NDRegisterAccessor<double>::buffer_2D[0][i] = value;
      }
      UA_Variant_delete(val);
    } else if (val->type == &UA_TYPES[UA_TYPES_DOUBLE]){
        UA_Variant_delete(val);
        throw ChimeraTK::runtime_error(std::string("OPC-UA-Backend::The variable: ") + _node_id + " is of type double. "
       "It will not be casted to float! Consider using the correct accessor.");
    } else {
      UA_Variant_delete(val);
      throw ChimeraTK::runtime_error(std::string("OPC-UA-Backend::The variable: ") + _node_id + " has an unsupported data type: " + val->type->typeName);
    }
  }

  template<>
  bool OpcUABackendRegisterAccessor<float>::doReadTransferLatest() {
    doReadTransfer();
    return true;
  }

  template<>
  bool OpcUABackendRegisterAccessor<float>::doReadTransferNonBlocking() {
    doReadTransfer();
    return true;
  }

  template<>
  bool OpcUABackendRegisterAccessor<float>::doWriteTransfer(ChimeraTK::VersionNumber){
    std::lock_guard<std::mutex> lock(opcua_mutex);
    UA_Variant *val = UA_Variant_new();
    if(_isScalar){
      UA_Variant_setScalarCopy(val, &NDRegisterAccessor<float>::buffer_2D[0][0], &UA_TYPES[UA_TYPES_FLOAT]);
    } else {
      UA_Variant_setArrayCopy(val, &NDRegisterAccessor<float>::buffer_2D[0][0], _arraySize,  &UA_TYPES[UA_TYPES_FLOAT]);
    }
    UA_StatusCode retval = UA_Client_writeValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);
    UA_Variant_delete(val);
    if(retval == UA_STATUSCODE_GOOD){
      return true;
    } else if (retval == UA_STATUSCODE_BADNOTWRITABLE || retval == UA_STATUSCODE_BADWRITENOTSUPPORTED){
      throw ChimeraTK::runtime_error(std::string("OPC-UA-Backend::Variable ") + _node_id + " is not writable!");
    } else {
//      printf("Failed writing with error code: %zu", retval);
      return false;
    }
  }
}


#endif /* OPC_UA_BACKENDREGISTERACCESSOR_H_ */
