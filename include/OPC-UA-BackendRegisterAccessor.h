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
#include "VersionMapper.h"

#include <sstream>

#include <boost/fusion/container/map.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/shared_ptr.hpp>

#include <mutex>
#include <chrono>

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
    , fusion::pair<UA_Byte, UA_DataType>
    , fusion::pair<UA_Boolean, UA_DataType>> myMap;

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
      return UA_String_fromChars(x.c_str());
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
    OpcUABackendRegisterAccessorBase(boost::shared_ptr<OpcUABackend> backend, OpcUABackendRegisterInfo* info):_backend(backend), _info(info){}
    // future_queue used to notify the TransferFuture about completed transfers
    cppext::future_queue<UA_DataValue> _notifications;

    boost::shared_ptr<OpcUABackend> _backend;

    UA_DataValue _data;

    OpcUABackendRegisterInfo* _info;

    bool _subscribed{false}; ///< Remember if a subscription was added.

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
        fusion::make_pair<UA_Byte>(UA_TYPES[UA_TYPES_BYTE]),
        fusion::make_pair<UA_Boolean>(UA_TYPES[UA_TYPES_BOOLEAN])};
    /**
     * Convert the actual UA_DataValue to a VersionNumber.
     * The VersionNumeber is constructed from the source time stamp.
     */
    ChimeraTK::VersionNumber convertToTimePoint(){
      /**
      * UA_DateTime is encoded as a 64-bit signed integer
      * which represents the number of 100 nanosecond intervals since January 1, 1601
      * (UTC)
      * */
      int64_t sourceTimeStampUnixEpoch = (_data.sourceTimestamp - UA_DATETIME_UNIX_EPOCH);
      std::chrono::duration<int64_t,std::nano> d(sourceTimeStampUnixEpoch*100);
      std::chrono::time_point<std::chrono::system_clock,std::chrono::duration<int64_t,std::nano> > tp(d);

      return ChimeraTK::VersionNumber(tp);
    }
  };

  template<typename UAType, typename CTKType>
  class OpcUABackendRegisterAccessor : public OpcUABackendRegisterAccessorBase, public NDRegisterAccessor<CTKType> {

  public:

//   virtual ~OpcUABackendRegisterAccessor(){this->shutdown();};
   ~OpcUABackendRegisterAccessor();

   void doReadTransferSynchronously() override;

   void doPreRead(TransferType) override {
     if(!_backend->isOpen()) {
       throw ChimeraTK::logic_error("Read operation not allowed while device is closed.");
     }
     // This will be done by the subscription manager how sends exception to the future queue and stops waiting.
//     if(_backend->isAsyncReadActive() && !OPCUASubscriptionManager::getInstance().isActive()){
//       throw ChimeraTK::runtime_error("SubscriptionManager error.");
//     }
   }

   void doPostRead(TransferType, bool /*hasNewData*/) override;

   void doPreWrite(TransferType, VersionNumber) override {
     if(!_backend->isOpen()) throw ChimeraTK::logic_error("Write operation not allowed while device is closed.");
   }

   bool doWriteTransfer(VersionNumber /*versionNumber*/={}) override;

   OpcUABackendRegisterAccessor(const RegisterPath &path, boost::shared_ptr<DeviceBackend> backend,const std::string &node_id, OpcUABackendRegisterInfo* registerInfo, AccessModeFlags flags, size_t numberOfWords, size_t wordOffsetInRegister);

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
   size_t _numberOfWords; ///< Requested array length. Could be smaller than what is available on the server.
   size_t _offsetWords; ///< Requested offset for arrays.
   RangeCheckingDataConverter<UAType, CTKType> toOpcUA;
   RangeCheckingDataConverter<CTKType, UAType> toCTK;
   bool _isPartial{false};

  private:

   void handleError(const UA_StatusCode &retval);
  };

  template<typename UAType, typename CTKType>
  OpcUABackendRegisterAccessor<UAType, CTKType>::OpcUABackendRegisterAccessor(const RegisterPath &path, boost::shared_ptr<DeviceBackend> backend, const std::string &node_id, OpcUABackendRegisterInfo* registerInfo,
      AccessModeFlags flags, size_t numberOfWords, size_t wordOffsetInRegister)
  : OpcUABackendRegisterAccessorBase(boost::dynamic_pointer_cast<OpcUABackend>(backend), registerInfo), NDRegisterAccessor<CTKType>(path, flags), _node_id(node_id), _numberOfWords(numberOfWords), _offsetWords(wordOffsetInRegister)
  {
    if(flags.has(AccessMode::raw))
      throw ChimeraTK::logic_error("Raw access mode is not supported.");

    NDRegisterAccessor<CTKType>::buffer_2D.resize(1);
    this->accessChannel(0).resize(numberOfWords);
    if(flags.has(AccessMode::wait_for_new_data)){
      UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                  "Adding subscription for node: %s", _info->_nodeBrowseName.c_str());
      // Create notification queue.
      _notifications = cppext::future_queue<UA_DataValue>(3);
      _readQueue = _notifications.then<void>([this](UA_DataValue& data) { this->_data = data; }, std::launch::deferred);
      // needs to be called after the notifications queue is created!
      if(!_backend->_subscriptionManager)
        _backend->activateSubscriptionSupport();
      _backend->_subscriptionManager->subscribe(_info->_nodeBrowseName, _info->_id, this);
      _subscribed = true;
    }
    if(_info->_arrayLength != numberOfWords)
      _isPartial = true;
    NDRegisterAccessor<CTKType>::_exceptionBackend = backend;
  }


  template<typename UAType, typename CTKType>
  void OpcUABackendRegisterAccessor<UAType, CTKType>::doReadTransferSynchronously() {
    if(!_backend->isFunctional()){
      throw ChimeraTK::runtime_error(std::string("Exception reported by another accessor."));
    }
    std::lock_guard<std::mutex> lock(_backend->_connection->client_lock);
    std::shared_ptr<ManagedVariant> val(new ManagedVariant());
    UA_StatusCode retval = UA_Client_readValueAttribute(_backend->_connection->client.get(), _info->_id, val->var);
    if(retval != UA_STATUSCODE_GOOD){
      handleError(retval);
    }

    // Write data to  the internal data buffer
    UA_Variant_copy(val->var, &_data.value);
    _data.sourceTimestamp = UA_DateTime_now();
  }

  template<typename UAType, typename CTKType>
  void OpcUABackendRegisterAccessor<UAType, CTKType>::doPostRead(TransferType, bool hasNewData) {
    if(!hasNewData) return;
    if(_data.status){
      UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                        "Data status error for node: %s Error: %s", _info->_nodeBrowseName.c_str(),  UA_StatusCode_name(_data.status));
      this->setDataValidity(DataValidity::faulty);
    } else {
      UAType* tmp = (UAType*)(_data.value.data);
      for(size_t i = 0; i < _numberOfWords; i++){
        UAType value = tmp[_offsetWords+i];
        // Fill the NDRegisterAccessor buffer
        this->accessData(i) = toCTK.convert(value);
      }
      this->setDataValidity(DataValidity::ok);
    }
    _currentVersion = VersionMapper::getInstance().getVersion(_data.sourceTimestamp);
    TransferElement::_versionNumber = _currentVersion;
  }

  template<typename UAType, typename CTKType>
  bool OpcUABackendRegisterAccessor<UAType, CTKType>::doWriteTransfer(ChimeraTK::VersionNumber versionNumber){
    if(!_backend->isFunctional()) {
      throw ChimeraTK::runtime_error(std::string("Exception reported by another accessor."));
    }
    UAType* arr;
    if(_isPartial){
      // read array first before changing only relevant parts of it
      OpcUABackendRegisterAccessor<UAType, CTKType>::doReadTransferSynchronously();
    }
    std::lock_guard<std::mutex> lock(_backend->_connection->client_lock);
    std::shared_ptr<ManagedVariant> val(new ManagedVariant());

    if(_isPartial){
      arr = (UAType*)(_data.value.data);
    } else {
      // create empty array
      arr = (UAType*)UA_Array_new(this->getNumberOfSamples(), &fusion::at_key<UAType>(m));
    }
    for(size_t i = 0; i < _numberOfWords; i++){
      arr[_offsetWords + i] = toOpcUA.convert(this->accessData(i));
    }
    if(_numberOfWords == 1){
      UA_Variant_setScalarCopy(val->var, arr, &fusion::at_key<UAType>(m));
    } else {
      UA_Variant_setArrayCopy(val->var, arr, _info->_arrayLength,  &fusion::at_key<UAType>(m));
    }
    UA_StatusCode retval = UA_Client_writeValueAttribute(_backend->_connection->client.get(), _info->_id, val->var);
    UA_Array_delete(arr, this->getNumberOfSamples(), &fusion::at_key<UAType>(m));
    _currentVersion = versionNumber;
    if(retval == UA_STATUSCODE_GOOD){
      return true;
    } else if (retval == UA_STATUSCODE_BADNOTWRITABLE || retval == UA_STATUSCODE_BADWRITENOTSUPPORTED){
      if(_backend->_subscriptionManager)
        _backend->_subscriptionManager->setExternalError(_info->_nodeBrowseName);
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
    if(_backend->_subscriptionManager)
      _backend->_subscriptionManager->setExternalError(_info->_nodeBrowseName);
    throw ChimeraTK::runtime_error(out.str());
  }

  template<typename UAType, typename CTKType>
  OpcUABackendRegisterAccessor<UAType, CTKType>::~OpcUABackendRegisterAccessor(){
    if(_subscribed){
      _backend->_subscriptionManager->unsubscribe(_info->_nodeBrowseName, this);
    }
  }
}


#endif /* OPC_UA_BACKENDREGISTERACCESSOR_H_ */
