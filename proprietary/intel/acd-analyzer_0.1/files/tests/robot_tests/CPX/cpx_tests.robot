##################################################################################
#
# INTEL CONFIDENTIAL
#
# Copyright 2021 Intel Corporation.
#
# This software and the related documents are Intel copyrighted materials, and
# your use of them is governed by the express license under which they were
# provided to you ("License"). Unless the License provides otherwise, you may
# not use, modify, copy, publish, distribute, disclose or transmit this
# software or the related documents without Intel's prior written permission.
#
# This software and the related documents are provided as is, with no express
# or implied warranties, other than those that are expressly stated in
# the License.
#
##################################################################################

*** Settings ***
Documentation       CPX crashdumps content tests
Library             ../../libs/crawler.py
Library             Process
Library             OperatingSystem
Library             String
Library             Collections
Test setup          Is Binary Present
Test Timeout        1 minute

*** Variables ***
${cpx_crashdump1}        tests/CPX/BMC_CPX_crashdump_sample_3strike.json
${cpx_crashdump1_bafi}   tests/CPX/BAFI_BMC_CPX_crashdump_sample_3strike.json
${cpx_crashdump2}        tests/CPX/crashdump_CPX6_IO.json
${cpx_crashdump2_bafi}   tests/CPX/BAFI_crashdump_CPX6_IO.json
${cpx_crashdump3}        tests/CPX/crashdump_CPX6_MMIO.json
${cpx_crashdump3_bafi}   tests/CPX/BAFI_crashdump_CPX6_MMIO.json

*** Test Cases ***
Cpx Test One
    [Documentation]                     This is a test comparing generated crashdump with BAFI_BMC_CPX_crashdump_sample_3strike.json

    ${result} =                         Run Command                             ${cpx_crashdump1}
    ${json} =                           evaluate                                json.loads('''${result}''')     json
    Dictionary Should Contain Key       ${json}                                 MCA
    Dictionary Should Contain Key       ${json}                                 TOR
    Dictionary Should Contain Key       ${json}                                 summary
    Dictionary Should Contain Key       ${json['summary']}                      socket0
    Dictionary Should Contain Key       ${json['summary']}                      socket1
    Dictionary Should Contain Key       ${json['summary']['socket0'][0]}        MCE
    Dictionary Should Contain Key       ${json['summary']['socket0'][1]}        Errors_Per_Socket
    Dictionary Should Contain Key       ${json['summary']['socket0'][2]}        First_MCERR
    Dictionary Should Contain Key       ${json['summary']['socket1'][0]}        Errors_Per_Socket
    ${json_differences} =               Check Differences                       ${result}       ${cpx_crashdump1_bafi}
    Should Be Empty                     ${json_differences}
    Run Keyword If                      '${json_differences}' != '''[]'''       Log To Console  ${json_differences}

Cpx Test Two
    [Documentation]                     This is a test comparing generated crashdump with BAFI_crashdump_CPX6_IO.json

    ${result} =                         Run Command                             ${cpx_crashdump2}
    ${json} =                           evaluate                                json.loads('''${result}''')     json
    Dictionary Should Contain Key       ${json}                                 MCA
    Dictionary Should Contain Key       ${json}                                 TOR
    Dictionary Should Contain Key       ${json}                                 summary
    Dictionary Should Contain Key       ${json['summary']}                      socket0
    Dictionary Should Contain Key       ${json['summary']['socket0'][0]}        MCE
    Dictionary Should Contain Key       ${json['summary']['socket0'][1]}        Errors_Per_Socket
    Dictionary Should Contain Key       ${json['summary']['socket0'][2]}        First_MCERR
    ${json_differences} =               Check Differences                       ${result}       ${cpx_crashdump2_bafi}
    Should Be Empty                     ${json_differences}
    Run Keyword If                      '${json_differences}' != '''[]'''       Log To Console  ${json_differences}

Cpx Test Three
    [Documentation]                     This is a test comparing generated crashdump with BAFI_crashdump_CPX6_MMIO.json

    ${result} =                         Run Command                             ${cpx_crashdump3}
    ${json} =                           evaluate                                json.loads('''${result}''')     json
    Dictionary Should Contain Key       ${json}                                 MCA
    Dictionary Should Contain Key       ${json}                                 TOR
    Dictionary Should Contain Key       ${json}                                 summary
    Dictionary Should Contain Key       ${json['summary']}                      socket0
    Dictionary Should Contain Key       ${json['summary']['socket0'][0]}        MCE
    Dictionary Should Contain Key       ${json['summary']['socket0'][1]}        Errors_Per_Socket
    Dictionary Should Contain Key       ${json['summary']['socket0'][2]}        First_MCERR
    ${json_differences} =               Check Differences                       ${result}       ${cpx_crashdump3_bafi}
    Should Be Empty                     ${json_differences}
    Run Keyword If                      '${json_differences}' != '''[]'''       Log To Console  ${json_differences}


*** Keywords ***
Run Command
    [Arguments]         ${crashdump_name}
    Log To Console      \n
    ${binary_path} =         Determine System
    ${result} =         Run    ${binary_path} ${crashdump_name}
    [Return]            ${result}