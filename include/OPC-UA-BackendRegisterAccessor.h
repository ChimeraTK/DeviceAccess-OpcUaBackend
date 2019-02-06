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

   OpcUABackendRegisterAccessor(const RegisterPath &path, UA_Client *client,const std::string &node_id);

   bool isReadOnly() const override {
     return _readonly;
   }

   bool isReadable() const override {
     return true;
   }

   bool isWriteable() const override {
     return !_readonly;
   }

   bool _readonly;

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
   size_t _arraySize;
   bool _isScalar;

  };

  template<typename UserType>
  OpcUABackendRegisterAccessor<UserType>::OpcUABackendRegisterAccessor(const RegisterPath &path, UA_Client *client, const std::string &node_id)
  : SyncNDRegisterAccessor<UserType>(path), _client(client), _node_id(node_id)
  {
    //\ToDo: Check if variable is array
    try {
      UA_Variant *val = UA_Variant_new();
      UA_StatusCode retval = UA_Client_readValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);
      // allocate buffers
      if(retval == UA_STATUSCODE_GOOD && UA_Variant_isScalar(val)){
        NDRegisterAccessor<UserType>::buffer_2D.resize(1);
        NDRegisterAccessor<UserType>::buffer_2D[0].resize(1);
        _arraySize = 1;
        _isScalar = true;
      } else if (retval == UA_STATUSCODE_GOOD ){
        _isScalar = false;
        NDRegisterAccessor<UserType>::buffer_2D.resize(1);
        if(val->arrayLength == 0){
          throw ChimeraTK::runtime_error("Array length is 0!");
        } else {
          printf("\nFound array of size: %d\n", (int)val->arrayLength);
          NDRegisterAccessor<UserType>::buffer_2D[0].resize(val->arrayLength);
          _arraySize = val->arrayLength;
        }
      }

      _readonly = false;
    }
    catch(...) {
      this->shutdown();
      std::cerr << "Shuting down..." << std::endl;
      throw;
    }
  }


  template<typename UserType>
  void OpcUABackendRegisterAccessor<UserType>::doReadTransfer() {
    throw ChimeraTK::runtime_error("Data type not supported by OneWireBackendRegisterAccessor.");
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
    throw ChimeraTK::runtime_error("Data type not supported by OneWireBackendRegisterAccessor.");
  }

  template<>
  void OpcUABackendRegisterAccessor<int32_t>::doReadTransfer() {
    std::lock_guard<std::mutex> lock(opcua_mutex);
//    std::cout << "Reading int value of node (1, \"" << _node_id << "\":" << std::endl;
    UA_Variant *val = UA_Variant_new();
    UA_StatusCode retval = UA_Client_readValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);
    if(retval != UA_STATUSCODE_GOOD){
      UA_Variant_delete(val);
      throw ChimeraTK::runtime_error(std::string("Failed to read data from variable: ") + _node_id);
    }
    if(val->type == &UA_TYPES[UA_TYPES_INT32]) {
      for(size_t i = 0; i < _arraySize; i++){
        UA_Int32* tmp = (UA_Int32*)val->data;
        UA_Int32 value = tmp[i];
        NDRegisterAccessor<int32_t>::buffer_2D[0][i] = value;
//        printf("the value is: %i\n", value);
      }
      UA_Variant_delete(val);
    } else {
      UA_Variant_delete(val);
      throw ChimeraTK::runtime_error(std::string("Type mismatch when reading variable: ") + _node_id);
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
      throw ChimeraTK::runtime_error(std::string("Variable ") + _node_id + " is not writable!");
    } else {
//      printf("Failed writing with error code: %zu", retval);
      return false;
    }
  }

  template<>
  void OpcUABackendRegisterAccessor<uint>::doReadTransfer() {
    std::lock_guard<std::mutex> lock(opcua_mutex);
//    std::cout << "Reading uint value of node (1, \"" << _node_id << "\":" << std::endl;
    UA_Variant *val = UA_Variant_new();
    UA_StatusCode retval = UA_Client_readValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);
    if(retval != UA_STATUSCODE_GOOD){
      UA_Variant_delete(val);
      throw ChimeraTK::runtime_error(std::string("Failed to read data from variable: ") + _node_id);
    }
    if(val->type == &UA_TYPES[UA_TYPES_UINT32]) {
      for(size_t i = 0; i < _arraySize; i++){
        UA_UInt32* tmp = (UA_UInt32*)val->data;
        UA_UInt32 value = tmp[i];
        NDRegisterAccessor<uint>::buffer_2D[0][i] = value;
//        printf("the value is: %i\n", value);
      }
      UA_Variant_delete(val);
    } else {
      UA_Variant_delete(val);
      throw ChimeraTK::runtime_error(std::string("Type mismatch when reading variable: ") + _node_id);
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
      throw ChimeraTK::runtime_error(std::string("Variable ") + _node_id + " is not writable!");
    } else {
//      printf("Failed writing with error code: %zu", retval);
      return false;
    }
  }

  template<>
  void OpcUABackendRegisterAccessor<std::string>::doReadTransfer() {
    std::lock_guard<std::mutex> lock(opcua_mutex);
//    std::cout << "Reading string value of node (1, \"" << _node_id << "\":" << std::endl;
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
//        printf("the value is: %-16.*s\n", value.length, value.data);
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
      throw ChimeraTK::runtime_error(std::string("Variable ") + _node_id + " is not writable!");
    } else {
//      printf("Failed writing with error code: %zu", retval);
      return false;
    }
  }

  template<>
  void OpcUABackendRegisterAccessor<double>::doReadTransfer() {
    std::lock_guard<std::mutex> lock(opcua_mutex);
//    std::cout << "Reading double value of node (1, \"" << _node_id << "\":" << std::endl;
    UA_Variant *val = UA_Variant_new();
    UA_StatusCode retval = UA_Client_readValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);
    if(retval != UA_STATUSCODE_GOOD){
      UA_Variant_delete(val);
      throw ChimeraTK::runtime_error(std::string("Failed to read data from variable: ") + _node_id);
    }
    if(val->type == &UA_TYPES[UA_TYPES_DOUBLE]) {
      for(size_t i = 0; i < _arraySize; i++){
        UA_Double* tmp = (UA_Double*)val->data;
        UA_Double value = tmp[i];
        NDRegisterAccessor<double>::buffer_2D[0][i] = value;
//        printf("the value is: %f\n", value);
      }
      UA_Variant_delete(val);
    } else {
      UA_Variant_delete(val);
      throw ChimeraTK::runtime_error(std::string("Type mismatch when reading variable: ") + _node_id);
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
      throw ChimeraTK::runtime_error(std::string("Variable ") + _node_id + " is not writable!");
    } else {
//      printf("Failed writing with error code: %zu", retval);
      return false;
    }
  }

  template<>
  void OpcUABackendRegisterAccessor<float>::doReadTransfer() {
    std::lock_guard<std::mutex> lock(opcua_mutex);
//    std::cout << "Reading float value of node (1, \"" << _node_id << "\":" << std::endl;
    UA_Variant *val = UA_Variant_new();
    UA_StatusCode retval = UA_Client_readValueAttribute(_client, UA_NODEID_STRING(1, const_cast<char*>(_node_id.c_str())), val);
    if(retval != UA_STATUSCODE_GOOD){
      UA_Variant_delete(val);
      throw ChimeraTK::runtime_error(std::string("Failed to read data from variable: ") + _node_id);
    }
    if(val->type == &UA_TYPES[UA_TYPES_FLOAT]) {
      for(size_t i = 0; i < _arraySize; i++){
        UA_Float* tmp = (UA_Float*)val->data;
        UA_Float value = tmp[i];
        NDRegisterAccessor<float>::buffer_2D[0][i] = value;
//        printf("the value is: %f\n", value);
      }
      UA_Variant_delete(val);
    } else {
      UA_Variant_delete(val);
      throw ChimeraTK::runtime_error(std::string("Type mismatch when reading variable: ") + _node_id);
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
      throw ChimeraTK::runtime_error(std::string("Variable ") + _node_id + " is not writable!");
    } else {
//      printf("Failed writing with error code: %zu", retval);
      return false;
    }
  }
}


#endif /* OPC_UA_BACKENDREGISTERACCESSOR_H_ */
