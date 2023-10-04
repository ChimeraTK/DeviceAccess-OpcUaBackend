// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
/*
 * VersionMapper.h
 *
 *  Created on: Jan 14, 2021
 *      Author: Klaus Zenker (HZDR)
 */
#include <open62541/types.h>

#include <ChimeraTK/VersionNumber.h>

#include <chrono>
#include <map>
#include <mutex>

using timePoint_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<int64_t, std::nano>>;

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
  VersionMapper();
  ~VersionMapper() = default;
  VersionMapper(const VersionMapper&) = delete;
  VersionMapper& operator=(const VersionMapper&) = delete;

  timePoint_t convertToTimePoint(const UA_DateTime& timeStamp);

  std::mutex _mapMutex;
  std::map<UA_DateTime, ChimeraTK::VersionNumber> _versionMap{};

  int64_t _localTimeOffset{0}; ///< Offest to be applied from UTC to local time [s]

  constexpr static size_t maxSizeEventIdMap = 2000;
};
