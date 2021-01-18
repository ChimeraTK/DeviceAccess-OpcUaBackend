/*
 * VersionMapper.h
 *
 *  Created on: Jan 14, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#ifndef INCLUDE_VERSIONMAPPER_H_
#define INCLUDE_VERSIONMAPPER_H_

#include <map>
#include <mutex>
#include <chrono>
#include <ChimeraTK/VersionNumber.h>
#include <open62541.h>


using timePoint_t = std::chrono::time_point<std::chrono::system_clock,std::chrono::duration<int64_t,std::nano> > ;

/**
 * This class is needed, because if two accessors are create and receive the same data from the server their
 * VersionNumber is required to be identical. Since the VersionNumber is not identical just because it is created
 * with the same timestamp this VersionMapper is needed. It takes care of holding a global map that connects source
 * time stamps attached  to OPC UA data to ChimeraTK::VersionNumber.
 *
 * It assumes that OPC UA source time stamps are unique.
 * \ToDo: Does every accessor needs it own VersionNumber history??
 */
class VersionMapper {
 public:
  static VersionMapper& getInstance() {
    static VersionMapper instance;
    return instance;
  }

  ChimeraTK::VersionNumber getVersion(const UA_DateTime& timeStamp);

 private:
  VersionMapper() = default;
  ~VersionMapper() = default;
  VersionMapper(const VersionMapper&) = delete;
  VersionMapper& operator=(const VersionMapper&) = delete;

  timePoint_t convertToTimePoint(const UA_DateTime& timeStamp);

  std::mutex _mapMutex;
  std::map<UA_DateTime, ChimeraTK::VersionNumber> _versionMap{};

  constexpr static size_t maxSizeEventIdMap = 2000;
};




#endif /* INCLUDE_VERSIONMAPPER_H_ */
