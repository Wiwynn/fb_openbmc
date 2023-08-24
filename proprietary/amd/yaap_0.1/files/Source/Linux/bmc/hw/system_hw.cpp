/******************************************************************************/
/*! \file bmc/hw/system_hw.cpp bmc:: system HW driver.
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

#include "system_hw.h"
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include "infrastructure/debug.h"

static const uint8_t yaapKey[] = { 0x76, 0x07, 0x97, 0x22, 0x99, 0x1C, 0x4D, 0x10, 0x9F, 0xAB, 0x43, 0x26, 0x70, 0xCB, 0x45, 0x67 };


bmc::SystemHw::SystemHw(int driver)
{
    fd = driver;
}

uint32_t bmc::SystemHw::getDeviceName(const char *& name)
{
    name = "bmc::Hw";
    return E_SUCCESS;
}

/***************************************************************/

uint32_t bmc::SystemHw::getYaapKey(const uint8_t *& key)
{
    key = yaapKey;
    return E_SUCCESS;
}

/***************************************************************/

uint32_t bmc::SystemHw::getFirmwareInfo(vector<yaap::hal::fwInfo_t>& firmwares)
{
    yaap::hal::fwInfo_t fwInfo;
    fwInfo.type = "os";
    fwInfo.majorVer = 1;
    fwInfo.minorVer = 8;
    fwInfo.pointVer = 1;

    firmwares.push_back(fwInfo);

    fwInfo.type = "fpga";
    fwInfo.majorVer = 1;
    fwInfo.minorVer = 5;
    fwInfo.pointVer = 0;

    firmwares.push_back(fwInfo);

    fwInfo.type = "numbatLpcSpi";
    fwInfo.majorVer = 1;
    fwInfo.minorVer = 1;
    fwInfo.pointVer = 0;

    firmwares.push_back(fwInfo);
    return E_SUCCESS;
}

/***************************************************************/

uint32_t bmc::SystemHw::getBoardInfo(vector<yaap::hal::boardInfo_t>& boards)
{
    yaap::hal::boardInfo_t board;

    board.name = "bmc::Board";
    board.partNo = "ABCD";
    board.revision = "1.0";
    board.serialNo = "0123456789";

    boards.push_back(board);

    board.name = "numbat";
    board.partNo = "ABCD";
    board.revision = "1.0";
    board.serialNo = "0123456789";

    boards.push_back(board);

    return E_SUCCESS;
}

/***************************************************************/

uint32_t bmc::SystemHw::lockChanged([[maybe_unused]] enum yaap::hal::lockChangeType changeType)
{
    return E_SUCCESS;
}

/***************************************************************/

uint32_t bmc::SystemHw::updateFirmware([[maybe_unused]] const char * fwType, [[maybe_unused]] uint8_t * fwData, [[maybe_unused]] size_t fwDataSize)
{
    return E_SUCCESS;
}

/***************************************************************/

uint32_t bmc::SystemHw::getFirmwareStatus([[maybe_unused]] const char * fwType, int& status, int& percent)
{
    status = 0;
    percent = 100;
    return E_SUCCESS;
}

/***************************************************************/

uint32_t bmc::SystemHw::reset(void)
{
    return E_SUCCESS;
}

uint32_t bmc::SystemHw::configMux(bool enable)
{
   DEBUG_PRINT(DEBUG_INFO, " configMux enable = %d",enable );

   if(enable)
   {
       //make high for BMC HDT
       setGPIOValue(jtagMuxSName, 0);
       setGPIOValue(jtagMuxOEName, 0);
       setGPIOValue(hdtSelName, 1);
   }
   else
   {
       //make low for external Wombat
       setGPIOValue(jtagMuxSName, 1);
       setGPIOValue(jtagMuxOEName, 1);
       setGPIOValue(hdtSelName, 0);
    }

    return E_SUCCESS;
}


