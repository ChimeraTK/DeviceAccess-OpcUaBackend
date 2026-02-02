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
  }

  ManagedDataValue::ManagedDataValue(const ManagedDataValue& other) {
    UA_DataValue_init(&_val);
    _val.status = UA_DataValue_copy(&other._val, &_val);
  }

  ManagedDataValue::ManagedDataValue(const ManagedDataValue&& other) noexcept {
    UA_DataValue_init(&_val);
    // copy variant
    _val.value.data = other._val.value.data;
    _val.value.arrayLength = other._val.value.arrayLength;
    _val.value.type = other._val.value.type;
    _val.value.storageType = other._val.value.storageType;
    _val.value.arrayDimensions = other._val.value.arrayDimensions;
    _val.value.arrayDimensionsSize = other._val.value.arrayDimensionsSize;
    // copy DataValue
    _val.hasStatus = other._val.hasStatus;
    _val.status = other._val.status;
    _val.hasServerTimestamp = other._val.hasServerTimestamp;
    _val.serverTimestamp = other._val.serverTimestamp;
    _val.hasSourceTimestamp = other._val.hasSourceTimestamp;
    _val.sourceTimestamp = other._val.sourceTimestamp;
    _val.hasValue = other._val.hasValue;
    _val.hasSourcePicoseconds = other._val.hasSourcePicoseconds;
    _val.sourcePicoseconds = other._val.sourcePicoseconds;
    _val.hasServerPicoseconds = other._val.hasServerPicoseconds;
    _val.serverPicoseconds = other._val.serverPicoseconds;
    if(_val.hasValue) {
      _clearData = true;
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
    // copy variant
    _val.value.data = other._val.value.data;
    _val.value.arrayLength = other._val.value.arrayLength;
    _val.value.type = other._val.value.type;
    _val.value.storageType = other._val.value.storageType;
    _val.value.arrayDimensions = other._val.value.arrayDimensions;
    _val.value.arrayDimensionsSize = other._val.value.arrayDimensionsSize;
    // copy DataValue
    _val.hasStatus = other._val.hasStatus;
    _val.status = other._val.status;
    _val.hasServerTimestamp = other._val.hasServerTimestamp;
    _val.serverTimestamp = other._val.serverTimestamp;
    _val.hasSourceTimestamp = other._val.hasSourceTimestamp;
    _val.sourceTimestamp = other._val.sourceTimestamp;
    _val.hasValue = other._val.hasValue;
    _val.hasSourcePicoseconds = other._val.hasSourcePicoseconds;
    _val.sourcePicoseconds = other._val.sourcePicoseconds;
    _val.hasServerPicoseconds = other._val.hasServerPicoseconds;
    _val.serverPicoseconds = other._val.serverPicoseconds;
    if(other.hasValue()) {
      _clearData = true;
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