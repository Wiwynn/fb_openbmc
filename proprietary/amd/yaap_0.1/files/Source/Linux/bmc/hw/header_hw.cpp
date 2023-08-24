/******************************************************************************/
/*! \file bmc/hw/header_hw.cpp bmc:: header HW driver
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

#include "header_hw.h"
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include "infrastructure/debug.h"

#include "aspeed.h"

bmc::HeaderHw::HeaderHw(int driver)
    : m_enabled(true)
{
    fd = driver;
    trstl = 0;
}

/***************************************************************/

uint32_t bmc::HeaderHw::isConnected(bool& connected, const char *& mode)
{
    connected = true;
    mode = HEADER_MODE_HDT_PLUS;
    return E_SUCCESS;
}

/***************************************************************/

uint32_t bmc::HeaderHw::isEnabled(bool& enabled)
{
    enabled = m_enabled;
    return E_SUCCESS;
}

/***************************************************************/

uint32_t bmc::HeaderHw::setEnabled([[maybe_unused]] bool enabled)
{

    return E_SUCCESS;
}

/***************************************************************/

uint32_t bmc::HeaderHw::isInConflict(bool& conflicted)
{
    conflicted = false;
    return E_SUCCESS;
}

/***************************************************************/

uint32_t bmc::HeaderHw::isChanged(bool& changed)
{
    changed = false;
    return E_SUCCESS;
}

/***************************************************************/

uint32_t bmc::HeaderHw::clearChanged(void)
{
    return E_SUCCESS;
}

uint32_t bmc::HeaderHw::trstSet(bool reset)
{
    int test = 0;
    struct jtag_trst_reset  trst_pin;

    DEBUG_PRINT(DEBUG_INFO, " trstSet.reset = %d ",reset);

    if(reset)
    {
        trst_pin.operation = 1;
        trst_pin.data      = 0; // 0 -> low on the pin
        test = ioctl (fd, JTAG_TRST_RESET, &trst_pin);
        if(test < 0)
        {
          DEBUG_PRINT(DEBUG_ERROR, "HeaderHw::trstSet Low failed, status: 0x%x. errno: %d.\n error message:%s", test, errno, strerror(errno));
          return E_SERVER_ERROR;
        }
    }
    else
    {
        trst_pin.operation = 1;
        trst_pin.data      = 1; // 1 -> high on the pin
        test = ioctl (fd, JTAG_TRST_RESET, &trst_pin);
        if(test < 0)
        {
          DEBUG_PRINT(DEBUG_ERROR, "HeaderHw::trstSet High failed, status: 0x%x. errno: %d.\n error message:%s", test, errno, strerror(errno));
          return E_SERVER_ERROR;
        }
    }
    return E_SUCCESS;
}

uint32_t bmc::HeaderHw::trstGet(bool& reset)
{
    int test = 0;
    struct jtag_trst_reset  trst_pin;

    trst_pin.operation = 0;
    test = ioctl (fd, JTAG_TRST_RESET, &trst_pin);
    if(test < 0)
    {
      DEBUG_PRINT(DEBUG_ERROR, "HeaderHw::trstGet failed, status: 0x%x. errno: %d.\n error message:%s", test, errno, strerror(errno));
      return E_SERVER_ERROR;
    }

    if(trst_pin.data == 0)
        reset = 1;
    else
        reset = 0;

    DEBUG_PRINT(DEBUG_INFO, " trstGet.reset = %d",reset );
    return E_SUCCESS;
}

/***************************************************************/
