/******************************************************************************/
/*! \file main.cpp YAAP Server test project main file.
 *
 * <pre>
 * Copyright (C) 2011-2020, ADVANCED MICRO DEVICES, INC.  All Rights Reserved.
 * AMD Confidential Proprietary
 *
 * AMD is granting you permission to use this software (the Materials)
 * pursuant to the terms and conditions of your Software License Agreement
 * with AMD.  This header does *NOT* give you permission to use the Materials
 * or any rights under AMD's intellectual property.  Your use of any portion
 * of these Materials shall constitute your acceptance of those terms and
 * conditions.  If you do not agree to the terms and conditions of the Software
 * License Agreement, please do not use any portion of these Materials.
 *
 * CONFIDENTIALITY:  The Materials and all other information, identified as
 * confidential and provided to you by AMD shall be kept confidential in
 * accordance with the terms and conditions of the Software License Agreement.
 *
 * LIMITATION OF LIABILITY: THE MATERIALS AND ANY OTHER RELATED INFORMATION
 * PROVIDED TO YOU BY AMD ARE PROVIDED "AS IS" WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY OF ANY KIND, INCLUDING BUT NOT LIMITED TO WARRANTIES OF
 * MERCHANTABILITY, NONINFRINGEMENT, TITLE, FITNESS FOR ANY PARTICULAR PURPOSE,
 * OR WARRANTIES ARISING FROM CONDUCT, COURSE OF DEALING, OR USAGE OF TRADE.
 * IN NO EVENT SHALL AMD OR ITS LICENSORS BE LIABLE FOR ANY DAMAGES WHATSOEVER
 * (INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS OF PROFITS, BUSINESS
 * INTERRUPTION, OR LOSS OF INFORMATION) ARISING OUT OF AMD'S NEGLIGENCE,
 * GROSS NEGLIGENCE, THE USE OF OR INABILITY TO USE THE MATERIALS OR ANY OTHER
 * RELATED INFORMATION PROVIDED TO YOU BY AMD, EVEN IF AMD HAS BEEN ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.  BECAUSE SOME JURISDICTIONS PROHIBIT THE
 * EXCLUSION OR LIMITATION OF LIABILITY FOR CONSEQUENTIAL OR INCIDENTAL DAMAGES,
 * THE ABOVE LIMITATION MAY NOT APPLY TO YOU.
 *
 * AMD does not assume any responsibility for any errors which may appear in
 * the Materials or any other related information provided to you by AMD, or
 * result from use of the Materials or any related information.
 *
 * You agree that you will not reverse engineer or decompile the Materials.
 *
 * NO SUPPORT OBLIGATION: AMD is not obligated to furnish, support, or make any
 * further information, software, technical information, know-how, or show-how
 * available to you.  Additionally, AMD retains the right to modify the
 * Materials at any time, without notice, and is not obligated to provide such
 * modified Materials to you.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS: The Materials are provided with
 * "RESTRICTED RIGHTS." Use, duplication, or disclosure by the Government is
 * subject to the restrictions as set forth in FAR 52.227-14 and
 * DFAR252.227-7013, et seq., or its successor.  Use of the Materials by the
 * Government constitutes acknowledgement of AMD's proprietary rights in them.
 *
 * EXPORT ASSURANCE:  You agree and certify that neither the Materials, nor any
 * direct product thereof will be exported directly or indirectly, into any
 * country prohibited by the United States Export Administration Act and the
 * regulations thereunder, without the required authorization from the U.S.
 * government nor will be used for any purpose prohibited by the same.
 * </pre>
 ******************************************************************************/

#include <fcntl.h>
#include <gpiod.hpp>
#include <system_error>

#include "infrastructure/server.h"
#include "classes/system.h"
#include "classes/device.h"
#include "classes/csJtag.h"
#include "classes/cpuDebug.h"
#include "classes/gpuDebug.h"
#include "classes/relay.h"
#include "classes/private.h"
#include "classes/lpc.h"
#include "classes/mux.h"
#include "classes/wombatUvdMux.h"


#include "hw/system_hw.h"
#include "hw/header_hw.h"
#include "hw/jtag_hw.h"
#include "hw/csjtag_hw.h"
#include "hw/triggers_hw.h"
#include "hw/hdt_hw.h"
#include "hw/relay_hw.h"
#include "hw/gpuDebug_hw.h"

#include "hw/lpcPostCode_hw.h"
#include "hw/lpcRomEmu_hw.h"

#include "hw/mux_hw.h"

#include "infrastructure/socketHandler.h"

#ifdef LIBYAAP_BIC
#include "yaap_bic/yaapBicWrap.h"
#endif

using namespace yaap;

const std::string hdtSelName = "HDT_SEL";
const std::string jtagMuxOEName = "JTAG_MUX_OE";
const std::string jtagMuxSName = "JTAG_MUX_S";
const std::string hdtDbgReqName = "HDT_DBREQ";
const std::string PwrOkName = "MON_PWROK";
const std::string boardID0Name = "BRD_ID_0";
const std::string boardID1Name = "BRD_ID_1";
const std::string boardID2Name = "BRD_ID_2";
const std::string boardID3Name = "BRD_ID_3";
const std::string powerButtonName = "ASSERT_PWR_BTN";
const std::string resetButtonName = "ASSERT_RST_BTN";
const std::string warmResetButtonName = "ASERT_WARM_RST_BTN";
const std::string fpgaReservedButtonName = "FPGA_RSVD";

#ifndef CONSUMER
#define CONSUMER "Consumer"
#endif

void usage(char * binary_name)
{
    static auto common_usage = "[ -v { 0 - 5 } ] [ -p TCP_PORT_NUMBER ] [ --pre TCK_PRE_SHIFT_CYCLES ] [ --post TCK_POST_SHIFT_CYCLES ] [ --timeout YAAP_TIMEOUT_TIME ] [ --default_jtag_hw_mode ]";
#ifdef LIBYAAP_BIC
    printf("\nusage: %s [ --slot <NID> ] [ -d ] %s\n", binary_name, common_usage);
    printf("\tNID: NODE ID\n");
#else
    printf("\nusage: %s %s\n", binary_name,common_usage);
    printf("\n");
#endif
}

int get_board_id(void)
{
    int board_id;
    board_id = (getGPIOValue(boardID3Name) << 3) |
               (getGPIOValue(boardID2Name) << 2) |
               (getGPIOValue(boardID1Name) << 1) |
               (getGPIOValue(boardID0Name) << 0);
    DEBUG_PRINT(DEBUG_INFO, "Board ID: 0x%x", board_id);

    return board_id;
}

int  getGPIOValue(const std::string& name)
{
    int value;
    gpiod::line gpioLine;

    // Find the GPIO line
    gpioLine = gpiod::find_line(name);
    if (!gpioLine)
    {
        DEBUG_PRINT(DEBUG_FATAL, "Can't find line: %s\n", name.c_str());
        return -1;
    }
    try
    {
        gpioLine.request({__FUNCTION__, gpiod::line_request::DIRECTION_INPUT, 0});
    }
    catch (std::system_error& exc)
    {
        DEBUG_PRINT(DEBUG_FATAL, "Error setting gpio as Input:: %s, Err Message:%s\n", name.c_str(), exc.what());
        return -1;
    }

    try
    {
        value = gpioLine.get_value();
    }
    catch (std::system_error& exc)
    {
        DEBUG_PRINT(DEBUG_FATAL, "Error getting gpio value for: %s, Err Message:%s\n", name.c_str(), exc.what());
        return -1;
    }

    return value;
}

bool setGPIOValue(const std::string& name, const int value)
{
    gpiod::line gpioLine;

    // Find the GPIO line
    gpioLine = gpiod::find_line(name);
    if (!gpioLine)
    {
        DEBUG_PRINT(DEBUG_FATAL, "Can't find line: %s\n", name.c_str());
        return false;
    }
    try
    {
        gpioLine.request({__FUNCTION__, gpiod::line_request::DIRECTION_OUTPUT, 0});
    }
    catch (std::system_error& exc)
    {
        DEBUG_PRINT(DEBUG_FATAL, "Error setting gpio as Output:: %s, Err Message:%s\n", name.c_str(), exc.what());
        return false;
    }

    try
    {
        // Request GPIO output to specified value
        gpioLine.set_value(value);
    }
    catch (std::exception& exc)
    {
        DEBUG_PRINT(DEBUG_FATAL, "Error setting gpio value for: %s, Err Message:%s\n", name.c_str(), exc.what());
        return false;
    }

    return true;
}

int main(int argc, char *argv[])
{
    int i;
    uint32_t preTck = 5;
    uint32_t postTck = 5;
    int yaapPort = DEFAULT_YAAP_PORT;
    int yaapTimeout = -1;
    int driver = -1;

#ifdef LIBYAAP_BIC
    int node_id = 0;
    yaap_bic::BicWrapConfig::getInstancePtr()->setDebugEnable(false);
#endif

    // Parse command line
    if (argc > 1) {
        for (i = 0; i < argc; i++) {
            if ((strcmp(argv[i], "-v") == 0) && ((i + 1) < argc)) {
                sscanf(argv[i + 1], "%i", &__debugLevel__);
                i++;
            } else if ((strcmp(argv[i], "-p") == 0) && ((i + 1) < argc)) {
                sscanf(argv[i + 1], "%i", &yaapPort);
                i++;
            } else if ((strcmp(argv[i], "--pre") == 0) && ((i + 1) < argc)) {
                sscanf(argv[i + 1], "%u", &preTck);
                i++;
            } else if ((strcmp(argv[i], "--post") == 0) && ((i + 1) < argc)) {
                sscanf(argv[i + 1], "%u", &postTck);
                i++;
            } else if ((strcmp(argv[i], "--timeout") == 0) && ((i + 1) < argc)) {
                sscanf(argv[i + 1], "%i", &yaapTimeout);
                i++;
            } else if (strcmp(argv[i], "--default_jtag_hw_mode") == 0)  {
                bmc::JtagHw::default_jtag_xfer_mode = JTAG_XFER_HW_MODE;
            } else if (strcmp(argv[i], "-h") == 0)  {
                usage(argv[0]);
                exit(0);
            }
            #ifdef LIBYAAP_BIC
            else if ((strcmp(argv[i], "--slot") == 0) && ((i + 1) < argc)) {
                sscanf(argv[i + 1], "%i", &node_id);
                // printf("Enable slot%d\n",node_id);
                yaap_bic::BicWrapConfig::getInstancePtr()->setFruId(node_id);
                i++;
            } else if (strcmp(argv[i], "-d") == 0) {
                // printf("Enable BIC debug mode\n");
                yaap_bic::BicWrapConfig::getInstancePtr()->setDebugEnable(true);
            }
            #endif
        }
    }

#ifdef LIBYAAP_BIC
    if (node_id <= 0) {
        fprintf(stderr, "Invalid Node Id: %d", node_id);
        usage(argv[0]);
        return -1;
    }
#else
    // System/board initialization code goes here...
    driver = open ("/dev/jtag0", O_RDWR);

    if (driver < 0) {
        printf("Can't open JTAG device /dev/jtag0!\n");
        DEBUG_PRINT(DEBUG_FATAL, "Can't open JTAG device /dev/jtag0!");
        return 0;
    }
#endif

    // Instantiate HW drivers (i.e. instances implementing HAL interfaces)...
    bmc::SystemHw systemHw(driver);

    // Instantiate YAAP classes.  Note that HDT expects specific names for the classes it interacts with.
    classes::system yaap_system("system", &systemHw);
    classes::device yaap_device("device", &systemHw);

    // The current implementation of `yaap_private` performs unsafe access to
    // virtual addresses within the BMC, which is not what is intended.  Disable
    // this.  We may need a better implementation of this on the BMC.
    classes::yaapPrivate yaap_private("private");



    bmc::JtagHw cpuDebugJtagHw(driver);
    bmc::HeaderHw cpuDebugHeaderHw(driver);
    bmc::TriggersHw triggersHw;
    bmc::HdtHw hdtHw(driver);


    bmc::csJtagHw cs1JtagHw(driver);
    bmc::csJtagHw cs2JtagHw(driver);
    bmc::HeaderHw cs1HeaderHw(driver);
    bmc::HeaderHw cs2HeaderHw(driver);


    bmc::RelayHw relay0Hw(driver,0);
    bmc::RelayHw relay1Hw(driver,1);
    bmc::RelayHw relay2Hw(driver,2);
    bmc::RelayHw relay3Hw(driver,3);

    bmc::LpcPostCodeHw lpcPostCodeHw;
    bmc::LpcRomEmulationHw lpcRomEmuHw;

    bmc::MuxHw mux0Hw;


    classes::cpuDebug yaap_cpuDebug("cpuDebug", &cpuDebugJtagHw, &cpuDebugHeaderHw, &triggersHw, &hdtHw);

    classes::csJtag yaap_csJtag1("csJtag1", &cs1JtagHw, &cs1HeaderHw);
    classes::csJtag yaap_csJtag2("csJtag2", &cs2JtagHw, &cs2HeaderHw);


    classes::relay yaap_relay0("relay0", &relay0Hw);
    classes::relay yaap_relay1("relay1", &relay1Hw);
    classes::relay yaap_relay2("relay2", &relay2Hw);
    classes::relay yaap_relay3("relay3", &relay3Hw);

    classes::lpc yaap_lpc("lpc", &lpcPostCodeHw, &lpcRomEmuHw);
    classes::wombatUvdMux uvdMux("wombatUvdMux", &mux0Hw);

    if (yaapTimeout > 0) {
        socketHandler::setConnectionTimeout(yaapTimeout);
    }

    // Start the YAAP server...
    return startServer(yaapPort, __debugLevel__);
}
