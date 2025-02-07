// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * VersionMapper.cc
 *
 *  Created on: Jan 14, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include "VersionMapper.h"

timePoint_t VersionMapper::convertToTimePoint(const UA_DateTime& timeStamp) {
  /**
   * UA_DateTime is encoded as a 64-bit signed integer
   * which represents the number of 100 nanosecond intervals since January 1, 1601
   * */
  int64_t sourceTimeStampUnixEpoch = (timeStamp - UA_DATETIME_UNIX_EPOCH);
  std::chrono::duration<int64_t, std::nano> d(sourceTimeStampUnixEpoch * 100);
  return timePoint_t(d);
}

ChimeraTK::VersionNumber VersionMapper::getVersion(const UA_DateTime& timeStamp) {
  std::lock_guard<std::mutex> lock(_mapMutex);
  if(!_versionMap.count(timeStamp)) {
    if(_versionMap.size() == maxSizeEventIdMap) _versionMap.erase(_versionMap.begin());
    _versionMap[timeStamp] = ChimeraTK::VersionNumber(convertToTimePoint(timeStamp));
  }
  return _versionMap[timeStamp];
}
