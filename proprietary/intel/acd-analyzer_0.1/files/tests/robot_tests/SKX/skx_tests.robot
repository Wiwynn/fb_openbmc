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
${skx_crashdump1}        tests/SKX/crashdump.json
${skx_crashdump1_bafi}   tests/SKX/BAFI_crashdump.json
${skx_crashdump2}        tests/SKX/crashdump_20191004_154251.json
${skx_crashdump2_bafi}   tests/SKX/BAFI_crashdump_20191004_154251.json
${skx_crashdump3}        tests/SKX/crashdump_r06s05_20181215_131834.json
${skx_crashdump3_bafi}   tests/SKX/BAFI_crashdump_r06s05_20181215_131834.json

*** Test Cases ***
Skx Test One
    [Documentation]                     This is a test comparing generated crashdump with BAFI_crashdump.json

    ${result} =                         Run Command                             ${skx_crashdump1}
    ${json} =                           evaluate                                json.loads('''${result}''')     json
    Dictionary Should Contain Key       ${json}                                 MCA
    Dictionary Should Contain Key       ${json}                                 TOR
    Dictionary Should Contain Key       ${json}                                 summary
    Dictionary Should Contain Key       ${json['summary']}                      socket0
    Dictionary Should Contain Key       ${json['summary']}                      socket1
    Dictionary Should Contain Key       ${json['summary']['socket0'][0]}        MCE
    Dictionary Should Contain Key       ${json['summary']['socket0'][1]}        Errors_Per_Socket
    Dictionary Should Contain Key       ${json['summary']['socket1'][0]}        MCE
    Dictionary Should Contain Key       ${json['summary']['socket1'][1]}        Errors_Per_Socket
    ${json_differences} =               Check Differences                       ${result}       ${skx_crashdump1_bafi}
    Should Be Empty                     ${json_differences}
    Run Keyword If                      '${json_differences}' != '''[]'''       Log To Console  ${json_differences}

Skx Test Two
    [Documentation]                     This is a test comparing generated crashdump with BAFI_crashdump_20191004_154251.json

    ${result} =                         Run Command                             ${skx_crashdump2}
    ${json} =                           evaluate                                json.loads('''${result}''')     json
    Dictionary Should Contain Key       ${json}                                 MCA
    Dictionary Should Contain Key       ${json}                                 TOR
    Dictionary Should Contain Key       ${json}                                 summary
    Dictionary Should Contain Key       ${json['summary']}                      socket0
    Dictionary Should Contain Key       ${json['summary']}                      socket1
    Dictionary Should Contain Key       ${json['summary']['socket0'][0]}        MCE
    Dictionary Should Contain Key       ${json['summary']['socket0'][1]}        TSC
    Dictionary Should Contain Key       ${json['summary']['socket0'][2]}        Errors_Per_Socket
    Dictionary Should Contain Key       ${json['summary']['socket1'][0]}        MCE
    Dictionary Should Contain Key       ${json['summary']['socket1'][1]}        TSC
    Dictionary Should Contain Key       ${json['summary']['socket1'][2]}        Errors_Per_Socket
    ${json_differences} =               Check Differences                       ${result}       ${skx_crashdump2_bafi}
    Should Be Empty                     ${json_differences}
    Run Keyword If                      '${json_differences}' != '''[]'''       Log To Console  ${json_differences}

Skx Test Three
    [Documentation]                     This is a test comparing generated crashdump with BAFI_crashdump_r06s05_20181215_131834.json

    ${result} =                         Run Command                             ${skx_crashdump3}
    ${json} =                           evaluate                                json.loads('''${result}''')     json
    Dictionary Should Contain Key       ${json}                                 MCA
    Dictionary Should Contain Key       ${json}                                 TOR
    Dictionary Should Contain Key       ${json}                                 summary
    Dictionary Should Contain Key       ${json['summary']}                      socket0
    Dictionary Should Contain Key       ${json['summary']}                      socket1
    Dictionary Should Contain Key       ${json['summary']['socket0'][0]}        MCE
    Dictionary Should Contain Key       ${json['summary']['socket0'][1]}        Errors_Per_Socket
    Dictionary Should Contain Key       ${json['summary']['socket1'][0]}        MCE
    Dictionary Should Contain Key       ${json['summary']['socket1'][1]}        Errors_Per_Socket
    ${json_differences} =               Check Differences                       ${result}       ${skx_crashdump3_bafi}
    Should Be Empty                     ${json_differences}
    Run Keyword If                      '${json_differences}' != '''[]'''       Log To Console  ${json_differences}


*** Keywords ***
Run Command
    [Arguments]         ${crashdump_name}
    Log To Console      \n
    ${binary_path} =         Determine System
    ${result} =         Run    ${binary_path} ${crashdump_name}
    [Return]            ${result}