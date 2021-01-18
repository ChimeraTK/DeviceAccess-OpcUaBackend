/*
 * VersionMapper.cc
 *
 *  Created on: Jan 14, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include "VersionMapper.h"
#include <iostream>

timePoint_t VersionMapper::convertToTimePoint(const UA_DateTime& timeStamp){
 /**
 * UA_DateTime is encoded as a 64-bit signed integer
 * which represents the number of 100 nanosecond intervals since January 1, 1601
 * (UTC)
 * */
 int64_t sourceTimeStampUnixEpoch = (timeStamp - UA_DATETIME_UNIX_EPOCH);
 std::chrono::duration<int64_t,std::nano> d(sourceTimeStampUnixEpoch*100);

 return timePoint_t(d);
}
ChimeraTK::VersionNumber VersionMapper::getVersion(const UA_DateTime &timeStamp){
  std::lock_guard<std::mutex> lock(_mapMutex);
  if(!_versionMap.count(timeStamp)){
    _versionMap[timeStamp] = ChimeraTK::VersionNumber(convertToTimePoint(timeStamp));
  }
  std::cout << "Source time stamp is: " << timeStamp << " assgined version is: " << (std::string)_versionMap[timeStamp] << std::endl;
  return _versionMap[timeStamp];
}


