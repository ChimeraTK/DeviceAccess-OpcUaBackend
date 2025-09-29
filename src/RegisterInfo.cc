// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * RegisterInfo.cc
 *
 *  Created on: Sep 18, 2025
 *      Author: Klaus Zenker (HZDR)
 */

#include "RegisterInfo.h"

#include <memory>
#include <string>

namespace ChimeraTK {
  void OpcUaBackendRegisterCatalogue::addProperty(const UA_NodeId& node, const std::string& browseName,
      const std::string& range, const UA_UInt32& dataType, const size_t& arrayLength, const std::string& serverAddress,
      const std::string& description, const bool isReadonly) {
    //    OpcUABackendRegisterInfo entry{serverAddress, browseName};
    //    UA_NodeId_copy(&node, &entry._id);
    OpcUABackendRegisterInfo entry{serverAddress, browseName, node};
    entry._dataType = dataType;
    entry._description = description;
    entry._arrayLength = arrayLength;
    entry._isReadonly = isReadonly;
    entry._accessModes.add(AccessMode::wait_for_new_data);
    entry._indexRange = range;
    // Maximum number of decimal digits to display a float without loss in non-exponential display, including
    // sign, leading 0, decimal dot and one extra digit to avoid rounding issues (hence the +4).
    // This computation matches the one performed in the NumericAddressedBackend catalogue.
    size_t floatMaxDigits =
        std::max(std::log10(std::numeric_limits<float>::max()), -std::log10(std::numeric_limits<float>::denorm_min())) +
        4;

    switch(entry._dataType) {
      case 1: /*BOOL*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::boolean, true, true, 320, 300);
        break;
      case 2: /*SByte aka int8*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, true, 4, 300);
        break;
      case 3: /*BYTE aka uint8*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, false, 3, 300);
        break;
      case 4: /*Int16*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, true, 5, 300);
        break;
      case 5: /*UInt16*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, false, 6, 300);
        break;
      case 6: /*Int32*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, true, 10, 300);
        break;
      case 7: /*UInt32*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, false, 11, 300);
        break;
      case 8: /*Int64*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, true, 320, 300);
        break;
      case 9: /*UInt64*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, true, false, 320, 300);
        break;
      case 10: /*Float*/
        entry.dataDescriptor =
            DataDescriptor(DataDescriptor::FundamentalType::numeric, false, true, floatMaxDigits, 300);
        break;
      case 11: /*Double*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::numeric, false, true, 300, 300);
        break;
      case 12: /*String*/
        entry.dataDescriptor = DataDescriptor(DataDescriptor::FundamentalType::string, true, true, 320, 300);
        break;
      default:
        return;
    }
    addRegister(entry);
  }
} // namespace ChimeraTK
