/******************************************************************************/
/*! \file main.cpp YAAP Server test project main file.
 *
 * <pre>
 * Copyright (C) 2011-2021, ADVANCED MICRO DEVICES, INC.  All Rights Reserved.
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

#ifdef FIRMWARE_SOFT_FUSE
#include <thread>
#endif
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
#ifdef FIRMWARE_SOFT_FUSE
#include "hw/psp_sfmon.h"
#endif

#include "infrastructure/socketHandler.h"

#define DEFAULT_TCK_FREQ	(5000000)
#define HDT_SEL_MUX_DELAY	(200000)

using namespace yaap;

const std::string hdtSelName = "HDT_SEL";
const std::string hdtDbgReqName = "HDT_DBREQ_L";
const std::string PwrOkName = "MON_POST_COMPLETE";

const std::string powerButtonName = "ASSERT_PWR_BTN_L";
const std::string resetButtonName = "ASSERT_RST_BTN_L";
const std::string warmResetButtonName = "ASSERT_WARM_RST_BTN_L";
const std::string fpgaReservedButtonName = "ASSERT_SOC_RST_BTN_L";
const std::string jtagTRSTName = "JTAG_TRST_N";

#ifndef CONSUMER
#define CONSUMER "Consumer"
#endif

bool is2PSystem = false;

int get_board_id(void)
{
    int board_id = 0xFF;

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
        gpioLine.request({__FUNCTION__, gpiod::line_request::DIRECTION_INPUT});
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
        gpioLine.request({__FUNCTION__, gpiod::line_request::DIRECTION_OUTPUT});
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

    if (name.compare(hdtSelName) == 0)
    {
        usleep(HDT_SEL_MUX_DELAY);
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
    bool jtag_sw_mode = false;

#ifdef FIRMWARE_SOFT_FUSE
    boost::asio::io_service io;
    const std::string pspSFName = "PSP_SOFT_FUSE_NOTIFY";
#endif

    // Parse command line
    if (argc > 1) {
        for (i = 0; i < argc; i++) {
            if ((strcmp(argv[i], "-v") == 0) && ((i + 1) < argc)) {
                sscanf(argv[i + 1], "%u", &__debugLevel__);
                i++;
            } else if ((strcmp(argv[i], "-p") == 0) && ((i + 1) < argc)) {
                sscanf(argv[i + 1], "%u", &yaapPort);
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
            } else if (strcmp(argv[i], "--jtag_sw_mode") == 0)  {
                jtag_sw_mode = true;
            } else if (strcmp(argv[i], "-h") == 0)  {
                printf("%s [ -v { 0 - 5 } ] [ -p TCP_PORT_NUMBER ] [ --pre TCK_PRE_SHIFT_CYCLES ] [ --post TCK_POST_SHIFT_CYCLES ] [ --timeout YAAP_TIMEOUT_TIME ] [ --jtag_sw_mode ]  \n", argv[0]);
                exit(0);
            }
        }
    }

    setGPIOValue(jtagTRSTName, 1);

    // System/board initialization code goes here...
    int driver = open ("/dev/jtag0", O_RDWR);

    if (driver < 0) {
        printf("Can't open JTAG device /dev/jtag0!\n");
        DEBUG_PRINT(DEBUG_FATAL, "Can't open JTAG device /dev/jtag0!");
        return 0;
    }

    // Instantiate HW drivers (i.e. instances implementing HAL interfaces)...
    bmc::SystemHw systemHw(driver);

    // Instantiate YAAP classes.  Note that HDT expects specific names for the classes it interacts with.
    classes::system yaap_system("system", &systemHw);
    classes::device yaap_device("device", &systemHw);
    classes::yaapPrivate yaap_private("private");



    bmc::JtagHw cpuDebugJtagHw(driver, DEFAULT_TCK_FREQ, jtag_sw_mode );
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


    classes::cpuDebug yaap_cpuDebug("cpuDebug", &cpuDebugJtagHw, &cpuDebugHeaderHw, &triggersHw, &hdtHw, preTck, postTck );

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
#ifdef JTAG_MODE_BMC_ONLY
    // Set BMC as default JTAG controller using mux
    DEBUG_PRINT(DEBUG_INFO, "Setting HDT SEL for BMC\n");
    setGPIOValue(hdtSelName, 1);
#endif

#ifdef FIRMWARE_SOFT_FUSE
    gpiod::line gpioLine;

    gpioLine = gpiod::find_line(pspSFName);
    if (!gpioLine)
    {
       	DEBUG_PRINT(DEBUG_FATAL, "Failed to find the: %s line\n", pspSFName.c_str());
        return false;
    }

    DEBUG_PRINT(DEBUG_INFO, "creating PSP Soft Fuse Notify Monitor Object\n");
    pspSfMonitor *pspSfMonObj = new pspSfMonitor(gpioLine, &cpuDebugJtagHw);
    std::thread th(&pspSfMonitor::requestGPIOEvents, pspSfMonObj);
    th.detach();
#endif

    // Start the YAAP server...
    return startServer(yaapPort, __debugLevel__);
}
