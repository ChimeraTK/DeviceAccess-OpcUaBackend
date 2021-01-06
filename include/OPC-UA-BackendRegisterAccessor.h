/*
 * OPC-UA-BackendRegisterAccessor.h
 *
 *  Created on: Nov 19, 2018
 *      Author: zenker
 */

#ifndef OPC_UA_BACKENDREGISTERACCESSOR_H_
#define OPC_UA_BACKENDREGISTERACCESSOR_H_

#include <ChimeraTK/NDRegisterAccessor.h>
#include <ChimeraTK/RegisterPath.h>
#include <ChimeraTK/AccessMode.h>
#include "OPC-UA-Backend.h"
#include "SubscriptionManager.h"

#include <sstream>

#include <boost/fusion/container/map.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/shared_ptr.hpp>

#include <mutex>

namespace fusion = boost::fusion;

namespace ChimeraTK {

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

  template <typename DestType, typename SourceType>
  class RangeCheckingDataConverter{
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

  //partial specialization of conversion to string
  template <typename SourceType>
  class RangeCheckingDataConverter<UA_String,SourceType>{
  public:
    UA_String convert(SourceType& x){
      return UA_STRING((char*)std::to_string(x).c_str());
    }
  };

  template <typename SourceType>
  class RangeCheckingDataConverter<std::string,SourceType>{
  public:
    std::string convert(SourceType& x){
      return std::to_string(x);
    }
  };


  //partial specialization of conversion to string
  template <>
  class RangeCheckingDataConverter<UA_String,std::string>{
  public:
    UA_String convert(std::string& x){
      return UA_STRING((char*)x.c_str());
    }
  };


  //partial specialization of conversion from string
  template <typename DestType>
  class RangeCheckingDataConverter<DestType, UA_String>{
  public:
    DestType convert(UA_String&){
      throw std::logic_error("Conversion from string is not allowed.");

    }
  };
  template <typename DestType>
  class RangeCheckingDataConverter<DestType, std::string>{
  public:
    DestType convert(std::string&){
      throw std::logic_error("Conversion from string is not allowed.");

    }
  };

  template <>
  class RangeCheckingDataConverter<std::string,UA_String>{
  public:
    std::string convert(UA_String& x){
      return std::string((char*)x.data, x.length);
    }
  };

  /**
   * Untemplated base class used in connection with the subscription manager.
   */
  class OpcUABackendRegisterAccessorBase {
  public:
    OpcUABackendRegisterAccessorBase(boost::shared_ptr<OpcUABackend> backend):_backend(backend){}
    /// future_queue used to notify the TransferFuture about completed transfers
    cppext::future_queue<UA_DataValue> _notifications;

    boost::shared_ptr<OpcUABackend> _backend;

    UA_DataValue _data;

    myMap m{
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
        fusion::make_pair<UA_Byte>(UA_TYPES[UA_TYPES_BYTE])};
  };

  template<typename UAType, typename CTKType>
  class OpcUABackendRegisterAccessor : public OpcUABackendRegisterAccessorBase, public NDRegisterAccessor<CTKType> {

  public:

//   virtual ~OpcUABackendRegisterAccessor(){this->shutdown();};
   virtual ~OpcUABackendRegisterAccessor(){};

   void doReadTransferSynchronously() override;

   void doPreRead(TransferType) override {
     if(!_backend->isOpen()) throw ChimeraTK::logic_error("Read operation not allowed while device is closed.");
   }

   void doPostRead(TransferType, bool /*hasNewData*/) override;

   void doPreWrite(TransferType, VersionNumber) override {
     if(!_backend->isOpen()) throw ChimeraTK::logic_error("Write operation not allowed while device is closed.");
   }

   bool doWriteTransfer(VersionNumber /*versionNumber*/={}) override;

   OpcUABackendRegisterAccessor(const RegisterPath &path, boost::shared_ptr<DeviceBackend> backend,const std::string &node_id, OpcUABackendRegisterInfo* registerInfo, AccessModeFlags flags);

   bool isReadOnly() const override {
     return _info->_isReadonly;
   }

   bool isReadable() const override {
     return true;
   }

   bool isWriteable() const override {
     return !_info->_isReadonly;
   }

   void interrupt() override { this->interrupt_impl(this->_notifications);}

   using TransferElement::_readQueue;


   std::vector< boost::shared_ptr<TransferElement> > getHardwareAccessingElements() override {
     return { boost::enable_shared_from_this<TransferElement>::shared_from_this() };
   }

   std::list<boost::shared_ptr<TransferElement> > getInternalElements() override {
     return {};
   }

   void replaceTransferElement(boost::shared_ptr<TransferElement> /*newElement*/) override {} // LCOV_EXCL_LINE

   friend class OpcUABackend;


   std::string _node_id;
   ChimeraTK::VersionNumber _currentVersion;
   OpcUABackendRegisterInfo* _info;
   RangeCheckingDataConverter<UAType, CTKType> toOpcUA;
   RangeCheckingDataConverter<CTKType, UAType> toCTK;

  private:

   void handleError(const UA_StatusCode &retval);
  };

  template<typename UAType, typename CTKType>
  OpcUABackendRegisterAccessor<UAType, CTKType>::OpcUABackendRegisterAccessor(const RegisterPath &path, boost::shared_ptr<DeviceBackend> backend, const std::string &node_id, OpcUABackendRegisterInfo* registerInfo,
      AccessModeFlags flags)
  : OpcUABackendRegisterAccessorBase(boost::dynamic_pointer_cast<OpcUABackend>(backend)), NDRegisterAccessor<CTKType>(path, flags), _node_id(node_id), _info(registerInfo)
  {
    NDRegisterAccessor<CTKType>::buffer_2D.resize(1);
    this->accessChannel(0).resize(_info->_arrayLength);
    if(flags.has(AccessMode::wait_for_new_data)){
      //\ToDo: Implement subscription here!
//      _backend->_manager->subscribe(UA_NODEID_STRING(1,&_node_id[0]),this);
      std::cout << "Adding subscription for node: " << _info->_nodeBrowseName << std::endl;
      OPCUASubscriptionManager::getInstance().subscribe(_info->_id, this);
      // Create notification queue.
      _notifications = cppext::future_queue<UA_DataValue>(3);
      _readQueue = _notifications.then<void>([this](UA_DataValue& data) { this->_data = data; }, std::launch::deferred);
//      std::cerr << "Subscriptions are not yet supported by the backend." << std::endl;
    }
  }


  template<typename UAType, typename CTKType>
  void OpcUABackendRegisterAccessor<UAType, CTKType>::doReadTransferSynchronously() {
    if(!_backend->isFunctional()){
      throw ChimeraTK::runtime_error(std::string("Exception reported by another accessor."));
    }
    std::lock_guard<std::mutex> lock(_backend->opcua_mutex);
    std::shared_ptr<ManagedVariant> val(new ManagedVariant());
    UA_StatusCode retval = UA_Client_readValueAttribute(_backend->_client, _info->_id, val->var);
    if(retval != UA_STATUSCODE_GOOD){
      handleError(retval);
    }

    // Write data to  the internal data buffer
    UA_Variant_copy(val->var, &_data.value);
  }

  template<typename UAType, typename CTKType>
  void OpcUABackendRegisterAccessor<UAType, CTKType>::doPostRead(TransferType, bool hasNewData) {
    UAType* tmp = (UAType*)(_data.value.data);
    for(size_t i = 0; i < _info->_arrayLength; i++){
      UAType value = tmp[i];
      // Fill the NDRegisterAccessor buffer
      this->accessData(i) = toCTK.convert(value);
    }

  }

  template<typename UAType, typename CTKType>
  bool OpcUABackendRegisterAccessor<UAType, CTKType>::doWriteTransfer(ChimeraTK::VersionNumber versionNumber){
    std::lock_guard<std::mutex> lock(_backend->opcua_mutex);
    std::shared_ptr<ManagedVariant> val(new ManagedVariant());
    std::vector<UAType> v(this->getNumberOfSamples());
    for(size_t i = 0; i < this->getNumberOfSamples(); i++){
      v[i] = toOpcUA.convert(this->accessData(i));
    }
    if(_info->_arrayLength == 1){
      UA_Variant_setScalarCopy(val->var, &v[0], &fusion::at_key<UAType>(m));
    } else {
      UA_Variant_setArrayCopy(val->var, &v[0], _info->_arrayLength,  &fusion::at_key<UAType>(m));
    }
    UA_StatusCode retval = UA_Client_writeValueAttribute(_backend->_client, _info->_id, val->var);
    _currentVersion = versionNumber;
    if(retval == UA_STATUSCODE_GOOD){
      return true;
    } else if (retval == UA_STATUSCODE_BADNOTWRITABLE || retval == UA_STATUSCODE_BADWRITENOTSUPPORTED){
      throw ChimeraTK::logic_error(std::string("OPC-UA-Backend::Variable ") + _node_id + " is not writable!");
    } else {
      handleError(retval);
      return false;
    }
  }

  template<typename UAType, typename CTKType>
  void OpcUABackendRegisterAccessor<UAType, CTKType>::handleError(const UA_StatusCode &retval){
    std::stringstream out;
    out << "OPC-UA-Backend::Failed to access variable: " << _node_id << " with reason: " <<
        UA_StatusCode_name(retval) << " --> " <<
        std::hex << retval;
    throw ChimeraTK::runtime_error(out.str());
  }
}


#endif /* OPC_UA_BACKENDREGISTERACCESSOR_H_ */
