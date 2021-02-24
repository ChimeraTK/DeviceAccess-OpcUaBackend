/*
 * VersionMapper.cc
 *
 *  Created on: Jan 14, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include "VersionMapper.h"

VersionMapper::VersionMapper(){
  // 11h is chosen because it is the only time point in GMT were the date is the same all over the globe
  const std::time_t epoch_plus_11h = 60 * 60 * 11;
  // convert to local time
  const int local_time = localtime(&epoch_plus_11h)->tm_hour;
  // convert to GMT
  const int gm_time = gmtime(&epoch_plus_11h)->tm_hour;
  // get the difference [-11,12]h
  const int tz_diff = local_time - gm_time;
  _localTimeOffset = tz_diff*3600;
}

timePoint_t VersionMapper::convertToTimePoint(const UA_DateTime& timeStamp){
 /**
 * UA_DateTime is encoded as a 64-bit signed integer
 * which represents the number of 100 nanosecond intervals since January 1, 1601
 * (UTC)
 * */
 int64_t sourceTimeStampUnixEpoch = (timeStamp - UA_DATETIME_UNIX_EPOCH);
 std::chrono::duration<int64_t,std::nano> d((sourceTimeStampUnixEpoch + _localTimeOffset)*100);

 return timePoint_t(d);
}
ChimeraTK::VersionNumber VersionMapper::getVersion(const UA_DateTime &timeStamp){
  std::lock_guard<std::mutex> lock(_mapMutex);
  if(!_versionMap.count(timeStamp)){
    if(_versionMap.size() == maxSizeEventIdMap)
      _versionMap.erase(_versionMap.begin());
    _versionMap[timeStamp] = ChimeraTK::VersionNumber(convertToTimePoint(timeStamp));
  }
  return _versionMap[timeStamp];
}
