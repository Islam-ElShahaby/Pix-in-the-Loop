*** Settings ***
Documentation     High-level integration tests for the ESP32-S3 as a complete system.
...               These scenarios combine multiple peripherals to model real-world behavior.
...
Library           HILClient.py    /dev/ttyACM0    115200
Suite Setup       Initialize And Flush
Suite Teardown    Terminate Hil System

*** Test Cases ***

Command Link Liveness (PING)
    [Documentation]    Isolates the bench->DUT command path with no other deps:
    ...                send PING on uart1, expect PONG. If THIS fails, the DUT is
    ...                not receiving on uart1 RX (GPIO18) -- check the bench
    ...                PA9 -> DUT GPIO18 wire and remove any leftover TestCases
    ...                loopback jumpers (PA9<->PA3, PA2<->PA10). All other UART
    ...                failures below are downstream of this.
    ${resp}=    Ask Dut Reply    1    PING\n    4
    Should Contain    ${resp}    PONG

Scenario 1: Overheating Motor Emergency Shutdown
    [Documentation]    Simulates a motor thermistor reading a high temperature.
    ...                The ESP32 reads the high value, asserts the FAULT GPIO, and shuts off PWM.
    [Teardown]    Set Pwm Duty Cycle    1    1000    0
    # The thermal-protection loop only runs while the system is RUNNING, so wake
    # it first. Drive PB2 low THEN high so it is a guaranteed rising edge on DUT
    # GPIO5 regardless of the pin's prior state.
    Write Gpio    B    2    0
    Sleep    20ms
    Write Gpio    B    2    1
    Sleep    100ms
    # Simulate overheating: drive bench PWM1 (PA8) -> RC filter -> DUT ADC (GPIO4) high.
    Set Pwm Duty Cycle    1    1000    1000  # 100% duty on PWM1/PA8 (simulates ~3.3V)
    Sleep    300ms
    # Fault GPIO (bench PB9 <- DUT GPIO10) should be asserted
    ${fault}=    Read Gpio    B    9
    Should Contain    ${fault}    1
    # Motor PWM (bench ADC IN4/PA4 <- DUT GPIO2 via RC) should have dropped to ~0 V
    ${pwm_adc}=    Sample Adc Channel    4
    ${val}=    Evaluate    int("${pwm_adc}".split("=>")[1].strip())
    Should Be True    ${val} < 200

Scenario 2: Intrusion Detection and Logging
    [Documentation]    Toggles the Tamper GPIO. Verifies Relay turns on and UART sends an alert.
    # Ensure relay is currently off (bench PB10 <- DUT GPIO11)
    ${relay}=    Read Gpio    B    10
    Should Contain    ${relay}    0
    # Clear stale RX, then drive ONE rising edge (bench PB7 -> DUT GPIO6)
    Flush Command Link
    Write Gpio    B    7    0
    Write Gpio    B    7    1
    # Check for UART alert on Command Link
    ${alert}=    Uart Receive Data    1    30    500
    Should Contain    ${alert}    ALERT: TAMPER DETECTED
    # Check Relay is now on
    ${relay}=    Read Gpio    B    10
    Should Contain    ${relay}    1

Scenario 3: Remote Sensor Telemetry Processing
    [Documentation]    Streams a simulated sensor payload via UART2. ESP32 parses and sets PWM.
    # Send TEMP:50 to UART2
    Uart Send Data    2    TEMP:50\n
    Sleep    100ms
    # The ESP32 should set PWM to 500 permille (50%).
    # We read ADC CH4 to verify (should be ~1.65V, ~2048 counts).
    ${pwm_adc}=    Sample Adc Channel    4
    ${val}=    Evaluate    int("${pwm_adc}".split("=>")[1].strip())
    Should Be True    1500 < ${val} < 2600

Scenario 4: System State Machine Boot and Sleep
    [Documentation]    Press the WAKE button, then send a SLEEP command.
    # Wake: drive PB2 low THEN high for a guaranteed rising edge on DUT GPIO5
    Write Gpio    B    2    0
    Sleep    20ms
    Write Gpio    B    2    1
    Sleep    100ms
    # Check Heartbeat (bench PB12 <- DUT GPIO12) toggles
    ${hb1}=    Read Gpio    B    12
    Sleep    500ms
    ${hb2}=    Read Gpio    B    12
    Should Not Be Equal    ${hb1}    ${hb2}
    
    # Send SLEEP command via UART1
    ${ack}=    Ask Dut Reply    1    SLEEP\n    10
    Should Contain    ${ack}    SLEEPOK
    
    # Check everything is off
    ${pwm_adc}=    Sample Adc Channel    4
    ${val}=    Evaluate    int("${pwm_adc}".split("=>")[1].strip())
    Should Be True    ${val} < 200
    
    ${hb1}=    Read Gpio    B    12
    Sleep    600ms
    ${hb2}=    Read Gpio    B    12
    Should Be Equal    ${hb1}    ${hb2}  # No toggling

*** Keywords ***

Initialize And Flush
    Initialize Hil System
    Flush Command Link

Ask Dut Reply
    [Documentation]    Drain stale RX, send a command, and read the reply. The DUT
    ...                frames on the idle gap after the burst, so the stripped
    ...                trailing newline does not matter.
    [Arguments]    ${channel}    ${cmd}    ${nbytes}    ${timeout}=500
    Flush Command Link
    Uart Send Data    ${channel}    ${cmd}
    ${resp}=    Uart Receive Data    ${channel}    ${nbytes}    ${timeout}
    RETURN    ${resp}

Flush Command Link
    [Documentation]    Drain any stale bytes sitting in the bench's RX ring buffer
    ...                (it is never flushed) so leftovers from a previous run are
    ...                not mistaken for a command reply. The DUT console/boot log
    ...                is on USB-Serial-JTAG, so it no longer leaks onto uart1.
    FOR    ${i}    IN RANGE    20
        ${chunk}=    Uart Receive Data    1    63    40
        ${payload}=    Set Variable    ${chunk.replace("UART DATA:", "").strip()}
        IF    "${payload}" == ""    BREAK
    END
