*** Settings ***
Documentation       ICX crashdumps tests
Library             ../../libs/crawler.py
Library             Process
Library             OperatingSystem
Library             String
Library             Collections
Test setup          Is Binary Present
Test Timeout        1 minute

*** Variables ***
${binary_path}           build/bin/./bafi
${icx_crashdump}         tests/general_resources/crashdump_EMCA2_break.json
${triage_info}           tests/general_resources/BAFI_crashdump_EMCA2_break.json

*** Test Cases ***
Version Display Test
    [Documentation]                     This is a test of current version display

    ${result} =         Run         ${binary_path} -v
    Should Contain      ${result}   BAFI version 1.46

Triage Error Information Display Test
    [Documentation]                     This is a test of triage info display

    ${result} =         Run         ${binary_path} -t ${icx_crashdump}
    ${triage_info} =    Get File    ${triage_info}  encoding=UTF-8
    Should Be Equal     ${result}   ${triage_info}

No Crashdump Provided Test
    [Documentation]                     This is a test of using flags without crashdump_name

    ${result} =         Run         ${binary_path} -t -r
    Should Contain      ${result}   No crashdump file provided


*** Keywords ***
Run Command
    [Arguments]         ${crashdump_name}
    Log To Console      \n
    ${result} =         Run    ${binary_path} ${crashdump_name}
    [Return]            ${result}