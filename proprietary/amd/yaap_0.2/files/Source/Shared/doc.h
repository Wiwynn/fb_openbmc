/*! \mainpage YAAP Server Core
 *
 * \section sec_copyright Copyright
 *
 * Copyright (C) 2012, ADVANCED MICRO DEVICES, INC.  All Rights Reserved.
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
 *
 * \section sec_intro Introduction
 *
 * This documentation covers the implementation of the YAAP server core software.
 * YAAP is a a remote procedure call protocol used for communication with debug modules connected to AMD silicon 
 * targets.
 *
 * \section sec_architecture Architecture
 *
 * The architecture of an embedded implementation of YAAP is shown in the following image:
 *
 * \image html yaap_stack.png
 * \image latex yaap_stack.eps
 *
 * The pieces of this diagram are explained below:
 *
 * \par Debug Host: 
 * This is a computer running compatible YAAP client debug software, such as HDT or ACE.
 *
 * \par YAAP Core:
 * The YAAP core software is common to all embedded implementations of YAAP.  It includes 
 * socket and connection-handling code, common YAAP object and method implementations, definition
 * of the hardware-abstraction layer, YAARP (line-level encoding) packing and unpacking, etc.  
 * It listens on a BSD socket for YAAP method calls from the debug host, and responds appropriately.
 * Note that the YAAP core software is not a complete application; rather it must be compiled into
 * the user application.  The YAAP core software has the following dependencies:
 * \li C++ compiler
 * \li Standard C++ library
 * \li Standard template library (uses <tt>vector</tt>)
 * \li BSD Sockets network API (<tt>socket()</tt>, <tt>bind()</tt>, etc.)
 *
 * \par User Application:
 * The user application consists of the YAAP Core (compiled in), as well as the low-level code,
 * startup code, etc.  It controls the instantiation of YAAP objects according to the services
 * that are to be offered, and launches the YAAP message handler in the YAAP Core code.
 * The YAAP message handler never returns.
 * <br><br>
 * YAAP is organized into classes, which encapsulate related functionality.  Every YAAP 
 * implementation must include an instance of yaap::classes::device named "device",
 * yaap::classes::system named "system", and yaap::classes::yaapPrivate named "private".  For
 * standard CPU/APU JTAG debug functionality, you must also include an instance of
 * yaap::classes::cpuDebug named "cpuDebug".  These constructors of these YAAP classes require
 * implementations of various HAL classes (see below).  Refer to 'main.cpp' in the sample
 * implementation for an example.
 *
 * \par HAL and YAAP Low-Level:
 * The Hardware Abstraction Layer (HAL) represents the interface between the common YAAP 
 * Core code and the implementation-specific low-level code, which knows how to interface 
 * with the debug module hardware.
 * <br><br>
 * The HAL API is specified using C++ classes.  Low-level implementations must derive from the
 * classes, and implement the required functionality.  Unless explicitly indicated in the API
 * documentation, HAL implementations must respect the exact value of all method parameters
 * (for example, number of bits to shift, etc.).  Some methods will allow a "best effort" 
 * implementation (such as setting a frequency), but those cases are explicitly documented
 * as such.  
 * <br><br>
 * All methods return an error code,
 * and any values which must be set by the callee are passed in by reference.  
 * Methods which must be implemented are declared pure virtual in the API; for optional 
 * methods the API interface class typically contains a default implementation which 
 * returns the E_NOT_IMPLEMENTED error code.  Many of the HAL classes include a member named
 * \a m_supportedFunctionality, which is used to advertise the presence of optional functionality
 * to the YAAP core.  Implementers must be carefule to properly set these flags in the constructors
 * of their implementations of the HAL objects.  To create a low-level implementation, include the
 * relevant HAL header file (i.e. "#include "hal/jtag.h"), and define a concrete class
 * implementing the interface(s) of interest.  
 * <br><br>
 * It is acceptable for a concrete class to implement more than one HAL interface; 
 * for example, a single concrete class may implement yaap::hal::jtag, yaap::hal::hdt, 
 * yaap::hal::triggers, and yaap::hal::header.  In this case, you may need to update 
 * references to m_supportedFunctionality to include a specific scope.
 * <br><br>
 * Note that HAL Method implementations may only return the error codes enumerated in the 
 * HAL documentation; these are the only errors that calling code is prepared to handle.  
 * Returning any other value may result in unexpected behavior.  
 * 
 * \par Debug Module Hardware:
 * This represents the debug hardware available on the system.  This may include interfaces
 * for JTAG, LPC, I2C, relays, GPIOs, etc.  The low-level software knows how to communicate
 * with this hardware.
 *
 */

/*! \namespace yaap
 * 
 *  \brief Contains the YAAP server code.
 */

/*! \namespace yaap::classes
 * 
 *  \brief Contains the YAAP class definitions and implementations.
 */

/*! \namespace yaap::hal
 * 
 *  \brief Contains the Hardware Abstraction Layer (HAL).
 */

/*! \namespace yaap::local
 * 
 *  \brief Contains the local access implementations of the YAAP classes.
 * 
 * The local access imlementations can be used to provide access to YAAP class 
 * functionality for code running in the same process as the YAAP server.  Instead
 * of having to build and parse YAARP messages, a local class, derived from the
 * corresponding YAAP class, can be used to provide access.
 */

/*! \namespace YAARP
 * 
 *  \brief Contains YAARP protocol implementation (line-level encoding).
 */
