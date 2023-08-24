/*
 *
 * Copyright 2017-present Facebook. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef YAAP_CLASSES_BICWRAPBASE_H
#define YAAP_CLASSES_BICWRAPBASE_H

#include <memory>
#include <vector>

#define E_SUCCESS                   0
#define E_BIC_BASE                  3000
#define E_BIC_IPMI_FAILED           E_BIC_BASE + 0x1

namespace yaap_bic {

typedef std::vector<uint8_t> BasicDataBuffer;

class BicWrapConfig;
typedef std::shared_ptr<BicWrapConfig> BicWrapConfigPtr;
class BicWrapConfig
{
public:
    BicWrapConfig(): m_fruid(1) {}
    ~BicWrapConfig() {}

    // set methods
    void setFruId(uint8_t fruid) { m_fruid = fruid; }
    void setDebugEnable(bool enable) { m_debug_enable = enable; }

    // get methods
    uint8_t getFruId() const { return m_fruid; };
    bool isDebugEnabled() const { return m_debug_enable; }

public:
    static BicWrapConfigPtr getInstancePtr()
    {
        if (s_instance_sptr == NULL)
        {
            s_instance_sptr = BicWrapConfigPtr(new BicWrapConfig());
        }

        return s_instance_sptr;
    }

private:
    static BicWrapConfigPtr s_instance_sptr;
    uint8_t                 m_fruid;
    bool                    m_debug_enable = false;
};


class BicWrapBackend;
typedef std::shared_ptr<BicWrapBackend> BicWrapBackendPtr;
class BicWrapBackend
{
public:
    BicWrapBackend(uint32_t fruid = 1);
    virtual ~BicWrapBackend();

    int sendYaapHeader(
        uint32_t lock_id, uint32_t method_cnt,
        uint32_t total_page, uint16_t &curr_page);

    int sendYaapMethodRequest(
        uint32_t instance_id, uint32_t method_id, uint32_t payload_sz,
        BasicDataBuffer payload,
        uint32_t total_page, uint16_t &curr_page);

    int getYaapMethodResponse(BasicDataBuffer& res_buf, uint8_t res_type = 0);

public: // static method
    static uint32_t checkTotalPages(BasicDataBuffer buffer);


private:
    uint8_t                 m_fruid;
};

bool sendRawData(int fd, BasicDataBuffer &buffer);
} // namespace bic

#endif //YAAP_CLASSES_BICWRAPBASE_H