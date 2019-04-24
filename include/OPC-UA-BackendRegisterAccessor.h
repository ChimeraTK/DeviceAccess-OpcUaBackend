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

#include <ChimeraTK/ControlSystemAdapter/TypeChangingDecorator.h>

#include <sstream>

#include <boost/fusion/container/map.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/shared_ptr.hpp>

namespace fusion = boost::fusion;

namespace ChimeraTK {
  std::mutex opcua_mutex;

  struct ManagedVariant{
    ManagedVariant(){
      var = UA_Variant_new();
    }
    ~ManagedVariant(){
      UA_Variant_delete(var);
    }

    UA_Variant* var;
  };

  typedef fusion::map<
      fusion::pair<UA_Int16, UA_DataType>
    , fusion::pair<UA_UInt16, UA_DataType>
    , fusion::pair<UA_Int32, UA_DataType>
    , fusion::pair<UA_UInt32, UA_DataType>
    , fusion::pair<UA_Int64, UA_DataType>
    , fusion::pair<UA_UInt64, UA_DataType>
    , fusion::pair<UA_Double, UA_DataType>
    , fusion::pair<UA_Float, UA_DataType>
    , fusion::pair<UA_String, UA_DataType>
    , fusion::pair<UA_SByte, UA_DataType>
    , fusion::pair<UA_Byte, UA_DataType>> myMap;

  myMap m(
      fusion::make_pair<UA_Int16>(UA_TYPES[UA_TYPES_INT16]),
      fusion::make_pair<UA_UInt16>(UA_TYPES[UA_TYPES_UINT16]),
      fusion::make_pair<UA_Int32>(UA_TYPES[UA_TYPES_INT32]),
      fusion::make_pair<UA_UInt32>(UA_TYPES[UA_TYPES_UINT32]),
      fusion::make_pair<UA_Int64>(UA_TYPES[UA_TYPES_INT64]),
      fusion::make_pair<UA_UInt64>(UA_TYPES[UA_TYPES_UINT64]),
      fusion::make_pair<UA_Double>(UA_TYPES[UA_TYPES_DOUBLE]),
      fusion::make_pair<UA_Float>(UA_TYPES[UA_TYPES_FLOAT]),
      fusion::make_pair<UA_String>(UA_TYPES[UA_TYPES_STRING]),
      fusion::make_pair<UA_SByte>(UA_TYPES[UA_TYPES_SBYTE]),
      fusion::make_pair<UA_Byte>(UA_TYPES[UA_TYPES_BYTE]));


  template <typename DestType, typename SourceType>
  class RangeChackingDataConverter{
    /** define round type for the boost::numeric::converter */
    template<class S>
    struct Round {
      static S nearbyint(S s) { return round(s); }

      typedef boost::mpl::integral_c<std::float_round_style, std::round_to_nearest> round_style;
    };
    typedef boost::numeric::converter<DestType, SourceType, boost::numeric::conversion_traits<DestType, SourceType>,
        boost::numeric::def_overflow_handler, Round<SourceType>>
        converter;
  public:
    DestType convert(SourceType& x){
      try{
        return converter::convert(x);
      } catch (boost::numeric::positive_overflow&) {
        return std::numeric_limits<DestType>::max();
      } catch (boost::numeric::negative_overflow&) {
        return std::numeric_limits<DestType>::min();
      }
    }
  };

  //partial specialisation of conversion to string
  template <typename SourceType>
  class RangeChackingDataConverter<UA_String,SourceType>{
  public:
    UA_String convert(SourceType& x){
      return UA_STRING((char*)std::to_string(x).c_str());
    }
  };

  template <typename SourceType>
  class RangeChackingDataConverter<std::string,SourceType>{
  public:
    std::string convert(SourceType& x){
      return std::to_string(x);
    }
  };


  //partial specialisation of conversion to string
  template <>
  class RangeChackingDataConverter<UA_String,std::string>{
  public:
    UA_String convert(std::string& x){
      return UA_STRING((char*)x.c_str());
    }
  };


  //partial specialisation of conversion from string
  template <typename DestType>
  class RangeChackingDataConverter<DestType, UA_String>{
  public:
    DestType convert(UA_String&){
      throw std::runtime_error("Conversion from string is not allowed.");

    }
  };
  template <typename DestType>
  class RangeChackingDataConverter<DestType, std::string>{
  public:
    DestType convert(std::string&){
      throw std::runtime_error("Conversion from string is not allowed.");

    }
  };

  template <>
  class RangeChackingDataConverter<std::string,UA_String>{
  public:
    std::string convert(UA_String& x){
      return std::string((char*)x.data, x.length);
    }
  };

template<typename UAType, typename CTKType>
  class OpcUABackendRegisterAccessor : public SyncNDRegisterAccessor<CTKType> {

  public:

   virtual ~OpcUABackendRegisterAccessor(){this->shutdown();};

   virtual void doReadTransfer() ;

   void doPostRead() override { _currentVersion = {}; }

   bool doReadTransferNonBlocking() override;

   bool doReadTransferLatest() override;

   virtual bool doWriteTransfer(ChimeraTK::VersionNumber /*versionNumber*/={}) override;

   AccessModeFlags getAccessModeFlags() const override {
     return {};
   }

   VersionNumber getVersionNumber() const override {
     return _currentVersion;
   }

   OpcUABackendRegisterAccessor(const RegisterPath &path, UA_Client *client,const std::string &node_id, OpcUABackendRegisterInfo* registerInfo);

   bool isReadOnly() const override {
     return _info->_isReadonly;
   }

   bool isReadable() const override {
     return true;
   }

   bool isWriteable() const override {
     return !_info->_isReadonly;
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
   ChimeraTK::VersionNumber _currentVersion;
   OpcUABackendRegisterInfo* _info;
   RangeChackingDataConverter<UAType, CTKType> toOpcUA;
   RangeChackingDataConverter<CTKType, UAType> toCTK;

  private:

   void handleError(const UA_StatusCode &retval);
  };

  template<typename UAType, typename CTKType>
  OpcUABackendRegisterAccessor<UAType, CTKType>::OpcUABackendRegisterAccessor(const RegisterPath &path, UA_Client *client, const std::string &node_id, OpcUABackendRegisterInfo* registerInfo)
  : SyncNDRegisterAccessor<CTKType>(path), _client(client), _node_id(node_id), _info(registerInfo)
  {
    NDRegisterAccessor<CTKType>::buffer_2D.resize(1);
    NDRegisterAccessor<CTKType>::buffer_2D[0].resize(_info->_arrayLength);
  }


  template<typename UAType, typename CTKType>
  void OpcUABackendRegisterAccessor<UAType, CTKType>::doReadTransfer() {
    std::lock_guard<std::mutex> lock(opcua_mutex);
    std::shared_ptr<ManagedVariant> val(new ManagedVariant());
    UA_StatusCode retval = UA_Client_readValueAttribute(_client, _info->_id, val->var);

    if(retval != UA_STATUSCODE_GOOD){
      handleError(retval);
    }

    UAType* tmp = (UAType*)val->var->data;
    for(size_t i = 0; i < _info->_arrayLength; i++){
      UAType value = tmp[i];
      // \ToDo: do proper conversion here!!
      NDRegisterAccessor<CTKType>::buffer_2D[0][i] = toCTK.convert(value);
    }
  }

  template<typename UAType, typename CTKType>
  bool OpcUABackendRegisterAccessor<UAType, CTKType>::doReadTransferLatest() {
    doReadTransfer();
    return true;
  }

  template<typename UAType, typename CTKType>
  bool OpcUABackendRegisterAccessor<UAType, CTKType>::doReadTransferNonBlocking() {
    doReadTransfer();
    return true;
  }

  template<typename UAType, typename CTKType>
  bool OpcUABackendRegisterAccessor<UAType, CTKType>::doWriteTransfer(ChimeraTK::VersionNumber versionNumber){
    std::lock_guard<std::mutex> lock(opcua_mutex);
    std::shared_ptr<ManagedVariant> val(new ManagedVariant());
    std::vector<UAType> v(NDRegisterAccessor<CTKType>::buffer_2D[0].size());
    for(size_t i = 0; i < NDRegisterAccessor<CTKType>::buffer_2D[0].size(); i++){
      v[i] = toOpcUA.convert(NDRegisterAccessor<CTKType>::buffer_2D[0][i]);
    }
    if(_info->_arrayLength == 1){
      UA_Variant_setScalarCopy(val->var, &v[0], &fusion::at_key<UAType>(m));
    } else {
      UA_Variant_setArrayCopy(val->var, &v[0], _info->_arrayLength,  &fusion::at_key<UAType>(m));
    }
    UA_StatusCode retval = UA_Client_writeValueAttribute(_client, _info->_id, val->var);
    _currentVersion = versionNumber;
    if(retval == UA_STATUSCODE_GOOD){
      return true;
    } else if (retval == UA_STATUSCODE_BADNOTWRITABLE || retval == UA_STATUSCODE_BADWRITENOTSUPPORTED){
      throw ChimeraTK::runtime_error(std::string("OPC-UA-Backend::Variable ") + _node_id + " is not writable!");
    } else {
      handleError(retval);
      return false;
    }
  }

  template<typename UAType, typename CTKType>
  void OpcUABackendRegisterAccessor<UAType, CTKType>::handleError(const UA_StatusCode &retval){
    std::stringstream out;
    out << "OPC-UA-Backend::Failed to access variable: " << _node_id << " with reason: " << std::hex << retval;
    throw ChimeraTK::runtime_error(out.str());
  }
}


#endif /* OPC_UA_BACKENDREGISTERACCESSOR_H_ */
