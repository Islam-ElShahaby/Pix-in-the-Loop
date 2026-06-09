*** Settings ***
Documentation     Automated Hardware-in-the-Loop Validation Suite for STM32 Black Pill.
Library           HILClient.py    /dev/ttyACM0    115200
Suite Setup       Initialize Hil System
Suite Teardown    Terminate Hil System

*** Test Cases ***
Verify DUT GPIO Input Polling
    [Documentation]    Validates driving the output pin high and low via custom protocol.
    ${response_high}=    Write Gpio    1
    Should Be Equal As Strings    ${response_high}    GPIO WRITE OK
    
    ${response_low}=     Write Gpio    0
    Should Be Equal As Strings    ${response_low}     GPIO WRITE OK

Verify DUT GPIO Output Controls
    [Documentation]    Queries the digital input pin and asserts valid protocol syntax.
    ${response}=    Read Gpio
    Should Start With    ${response}    GPIO VALUE: