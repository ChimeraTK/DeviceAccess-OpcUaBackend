// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * ManagedTypes.cc
 *
 *  Created on: Jan 28, 2026
 *      Author: Klaus Zenker (HZDR)
 */

#include "ManagedTypes.h"

#include <open62541/types.h>
#include <open62541/types_generated_handling.h>

namespace ChimeraTK {
  ManagedVariant::ManagedVariant() {
    var = UA_Variant_new();
  }
  ManagedVariant::~ManagedVariant() {
    UA_Variant_delete(var);
  }

  ManagedDataValue::ManagedDataValue() {
    UA_DataValue_init(&_val);
  }

  ManagedDataValue::ManagedDataValue(UA_DataValue* data) {
    UA_DataValue_init(&_val);
    UA_DataValue_copy(data, &_val);
    _val.hasValue = true;
  }

  ManagedDataValue::ManagedDataValue(const ManagedDataValue& other) {
    UA_DataValue_init(&_val);
    _val.status = UA_DataValue_copy(&other._val, &_val);
    _clearData = true;
  }

  ManagedDataValue::ManagedDataValue(const ManagedDataValue&& other) noexcept {
    UA_DataValue_init(&_val);
    if(other.hasValue()) {
      // set data pointer to the others data pointer
      _val.value.data = other._val.value.data;
      _val.value.arrayLength = other._val.value.arrayLength;
      _val.value.type = other._val.value.type;
      _val.hasValue = true;
      if(other._val.hasSourceTimestamp) {
        _val.hasSourceTimestamp = true;
        _val.sourceTimestamp = other._val.sourceTimestamp;
      }
      if(other._val.hasServerTimestamp) {
        _val.hasServerTimestamp = true;
        _val.sourceTimestamp = other._val.serverTimestamp;
      }
      _clearData = true;
    }
    else {
      _val.hasValue = false;
    }
  }

  ManagedDataValue::~ManagedDataValue() {
    if(hasValue() && _clearData) {
      UA_DataValue_clear(&_val);
      _val.hasValue = false;
    }
  }

  ManagedDataValue& ManagedDataValue::operator=(ManagedDataValue&& other) noexcept {
    prepare();
    if(other.hasValue()) {
      // set data pointer to the others data pointer
      _val.value.data = other._val.value.data;
      _val.value.arrayLength = other._val.value.arrayLength;
      _val.value.type = other._val.value.type;
      _clearData = true;
      _val.hasValue = true;
      if(other._val.hasSourceTimestamp) {
        _val.hasSourceTimestamp = true;
        _val.sourceTimestamp = other._val.sourceTimestamp;
      }
      if(other._val.hasServerTimestamp) {
        _val.hasServerTimestamp = true;
        _val.sourceTimestamp = other._val.serverTimestamp;
      }
    }
    else {
      _val.hasValue = false;
    }
    other._clearData = false;
    return *this;
  }

  void ManagedDataValue::copyVariant(const UA_Variant& src, const std::string& dataRange) {
    prepare();
    UA_StatusCode ret;
    if(dataRange.empty()) {
      ret = UA_Variant_copy(&src, &_val.value);
    }
    else {
      UA_NumericRange range = UA_NUMERICRANGE(dataRange.c_str());
      _val.status = UA_Variant_copyRange(&src, &_val.value, range);
      _val.hasStatus = true;
      UA_free(range.dimensions);
    }
    if(_val.status == UA_STATUSCODE_GOOD) {
      _val.hasValue = true;
      _val.sourceTimestamp = UA_DateTime_now();
      _val.hasSourceTimestamp = true;
    }
    _clearData = true;
  }

  void ManagedDataValue::prepare() {
    if(hasValue()) {
      UA_DataValue_clear(&_val);
      _val.hasValue = false;
    }
  }

} // namespace ChimeraTK