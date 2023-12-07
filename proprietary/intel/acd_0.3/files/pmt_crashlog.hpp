/******************************************************************************
 *
 * INTEL CONFIDENTIAL
 *
 * Copyright 2023 Intel Corporation.
 *
 * This software and the related documents are Intel copyrighted materials,
 * and your use of them is governed by the express license under which they
 * were provided to you ("License"). Unless the License provides otherwise,
 * you may not use, modify, copy, publish, distribute, disclose or transmit
 * this software or the related documents without Intel's prior written
 * permission.
 *
 * This software and the related documents are provided as is, with no express
 * or implied warranties, other than those that are expressly stated in the
 * License.
 *
 ******************************************************************************/

#pragma once

#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <vector>

extern "C" {
#include <cjson/cJSON.h>

#include "engine/Crashlog.h"
#include "engine/CrashlogInternal.h"
}

#include "utils_dbusplus.hpp"

#include <pmt/transport/transport.hpp>

void collectPMTCrashlog(cJSON* const pJsonChild, struct pmtHandler* pmtbind);
class PMTCrashlog
{
  private:
    bool nacCrashlogInProcess = false;
    std::unique_ptr<pmt::transport::Transport> mctpTransport;
    std::chrono::time_point<std::chrono::system_clock> lastUpdTime;
    std::set<std::shared_ptr<pmt::transport::Endpoint>> endpoints{};
    inline void setNACFlag(bool status);
    void pmtCollectCrash(cJSON* const pJsonChild, boost::asio::io_service& io);
    bool getCrashDetails(boost::asio::yield_context yield,
                         std::shared_ptr<pmt::transport::Endpoint> endpoint,
                         size_t& crashlogAgentIndex,
                         crashlogAgentDetails& agentDetails,
                         const agentsInfoInputFile& agentInfo);
    bool getCrashLog(boost::asio::yield_context yield,
                     std::shared_ptr<pmt::transport::Endpoint> endpoint,
                     size_t crashlogAgentIndex, size_t crashlogSampleCount,
                     std::vector<uint64_t>& sampleVec);

  public:
    PMTCrashlog(std::shared_ptr<sdbusplus::asio::connection> conn);
    void initPMTDiscovery(boost::asio::io_service& io);
    void startPMTCrashlog(cJSON* const pJsonChild,
                          boost::asio::io_service& ioc);
};
