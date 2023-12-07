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

#include "pmt_crashlog.hpp"

#include <pmt/transport/mctp/factory.hpp>

constexpr size_t guidNACIp = 0x10616980;

void PMTCrashlog::setNACFlag(bool status)
{
    nacCrashlogInProcess = status;
}

bool PMTCrashlog::getCrashLog(
    boost::asio::yield_context yield,
    std::shared_ptr<pmt::transport::Endpoint> endpoint,
    size_t crashlogAgentIndex, size_t crashlogSampleCount,
    std::vector<uint64_t>& sampleVec)
{
    for (size_t sampleId = 0; sampleId < crashlogSampleCount; sampleId++)
    {
        const auto& [status, sample] = mctpTransport->getCrashlogSample(
            yield, endpoint, crashlogAgentIndex, sampleId);
        if (status != pmt::transport::Status::success)
        {
            return false;
        }
        sampleVec.push_back(sample);
    }
    return true;
}

bool PMTCrashlog::getCrashDetails(
    boost::asio::yield_context yield,
    std::shared_ptr<pmt::transport::Endpoint> endpoint,
    size_t& crashlogAgentIndex, crashlogAgentDetails& agentDetails,
    const agentsInfoInputFile& agentInfo)
{
    std::pair<pmt::transport::Status, pmt::transport::CrashLogDetails>
        getCrashlogDetailsResponse;
    pmt::transport::CrashLogDetails details;
    getCrashlogDetailsResponse =
        mctpTransport->getCrashLogDetails(yield, endpoint, crashlogAgentIndex);
    if (getCrashlogDetailsResponse.first != pmt::transport::Status::success)
    {
        return false;
    }

    details = getCrashlogDetailsResponse.second;

    if (details.guid != agentInfo.agentMap->guid)
    {
        return false;
    }
    CRASHDUMP_PRINT(DEBUG, stderr, "NAC Agent Guid is matched\n");
    agentDetails.uniqueId = details.guid;

    agentDetails.crashSpace =
        details.size /
        sizeof(uint32_t); // details.size in bytes so converting into Dwords
    agentDetails.crashType = static_cast<uint8_t>(details.crashLogType);
    agentDetails.entryType = static_cast<uint8_t>(details.entryType);
    return true;
}

void PMTCrashlog::pmtCollectCrash(cJSON* const pJsonChild,
                                  boost::asio::io_service& io)
{
    boost::asio::steady_timer timer(io);
    lastUpdTime = std::chrono::system_clock::now();
    std::set<std::shared_ptr<pmt::transport::Endpoint>> endPointList =
        endpoints;
    size_t minimumProcessDelayInSec = 6;
    setNACFlag(true);

    boost::asio::spawn(io, [this, endPointList,
                            pJsonChild](boost::asio::yield_context yield) {
        size_t crashlogIndexCounter = 0;
        std::pair<pmt::transport::Status, uint16_t> crashlogCount;
        agentsInfoInputFile agentInfo;
        auto agtMap = std::make_unique<guidCrashlogSectionMapping>();
        agentInfo.agentMap = agtMap.get();
        agentInfo.agentMap->enable = true;
        agentInfo.agentMap->guid = guidNACIp;

        for (auto const& endpoint : endPointList)
        {
            lastUpdTime = std::chrono::system_clock::now();
            crashlogCount = mctpTransport->getCrashLogCount(yield, endpoint);
            if (crashlogCount.first != pmt::transport::Status::success)
            {
                continue;
            }

            for (size_t crashlogAgentIndex = 0;
                 crashlogAgentIndex < crashlogCount.second;
                 crashlogAgentIndex++)
            {
                std::vector<uint64_t> sampleVec;
                crashlogAgentDetails agentDetails;
                agentInfo.numberOfCrashlogAgents = crashlogCount.second;
                if (!getCrashDetails(yield, endpoint, crashlogAgentIndex,
                                     agentDetails, agentInfo))
                {
                    continue;
                }
                // agentDetails->crashSpace is the size in DWORDs and each
                // getCrashLog returns 2 DWORDs, so we only need half that
                // number of samples
                size_t crashlogSampleCount =
                    agentDetails.crashSpace / sizeof(uint16_t);
                if (!getCrashLog(yield, endpoint, crashlogAgentIndex,
                                 crashlogSampleCount, sampleVec))
                {
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "Error while collecting the Crashlog from %s\n",
                        (endpoint->str()).c_str());
                    continue;
                }

                if (!nacCrashlogInProcess)
                {
                    return;
                }

                std::string crashlogIndex =
                    "PMT_CRASH_INDEX_" + std::to_string(crashlogIndexCounter++);
                agentInfo.agentMap->crashlogSectionName = crashlogIndex.data();

                if (storeCrashlog(pJsonChild, crashlogAgentIndex, &agentDetails,
                                  sampleVec.data(), &agentInfo) != ACD_SUCCESS)
                {
                    CRASHDUMP_PRINT(
                        ERR, stderr,
                        "Error while storing PMT crashlog details for %s\n",
                        (endpoint->str()).c_str());
                    return;
                }
            }
        }
    });

    // We use blocked method of collecting crashlog since the library does not
    // support async mechanisms.
    while (true)
    {
        timer.expires_after(std::chrono::milliseconds(500));
        timer.wait();
        if ((std::chrono::system_clock::now() - lastUpdTime) >
            std::chrono::seconds(minimumProcessDelayInSec))
        {
            CRASHDUMP_PRINT(DEBUG, stderr, "PMT Processing time completed\n");
            break;
        }
    }
}

void PMTCrashlog::startPMTCrashlog(cJSON* const pJsonChild,
                                   boost::asio::io_service& ioc)
{
    if (endpoints.empty())
    {
        CRASHDUMP_PRINT(DEBUG, stderr, "No PMT capable device is registered\n");
        return;
    }
    if (nacCrashlogInProcess)
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "NAC Crashlog is already in the process\n");
        return;
    }
    pmtCollectCrash(pJsonChild, ioc);
    setNACFlag(false);
}

void collectPMTCrashlog(cJSON* const pJsonChild, struct pmtHandler* pmtbinding)
{
    boost::asio::io_service* ioc =
        reinterpret_cast<boost::asio::io_service*>(pmtbinding->ioc);
    PMTCrashlog* pmtCrashLog =
        reinterpret_cast<PMTCrashlog*>(pmtbinding->pmtObjptr);
    pmtCrashLog->startPMTCrashlog(pJsonChild, *ioc);
}

void PMTCrashlog::initPMTDiscovery(boost::asio::io_service& io)
{
    pmtHandler nacBinding;
    nacBinding.pmtObjptr = reinterpret_cast<void*>(this);
    nacBinding.ioc = reinterpret_cast<void*>(&io);
    nacBinding.fn = &collectPMTCrashlog;
    registerPMTCallback(&nacBinding);
}

PMTCrashlog::PMTCrashlog(std::shared_ptr<sdbusplus::asio::connection> conn) :
    mctpTransport(pmt::transport::mctp::makeMultiTransport(conn))
{
    mctpTransport->initDiscovery(
        [this](boost::asio::yield_context,
               std::shared_ptr<pmt::transport::Endpoint> endpoint,
               pmt::transport::Event event) {
            if (event == pmt::transport::Event::added)
            {
                endpoints.insert(endpoint);
            }
            else if (event == pmt::transport::Event::removed)
            {
                auto it =
                    std::find(endpoints.begin(), endpoints.end(), endpoint);
                if (it != endpoints.end())
                {
                    endpoints.erase(it);
                }
            }
        });
}
