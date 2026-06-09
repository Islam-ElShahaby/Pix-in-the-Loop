*** Settings ***
Documentation     Automated Hardware-in-the-Loop Validation Suite for STM32 Blackpill.
...               Uses the Zephyr Shell protocol: ctrl <command> [args...]
...
...               Wiring assumed by this suite:
...                 * PB2  -> GND   (reads 0)
...                 * PB7  -> 3.3V  (reads 1)
...                 * SPI1 (PB3/PB4/PB5) <-> SPI2 (PB13/PB14/PB15) loopback
...                 * USART1 <-> USART2 crossover: PA9 (TX1) -> PA3 (RX2)
...                   and PA2 (TX2) -> PA10 (RX1)
...
...               Firmware currently implements: gpio_set, gpio_get, pwm_set,
...               spi_write, spi_slave_wait, uart_send. ADC shell commands are
...               not yet implemented, so that suite is intentionally absent.
Library           HILClient.py    /dev/ttyACM0    115200
Suite Setup       Initialize Hil System
Suite Teardown    Terminate Hil System

*** Test Cases ***

# ---------------------------------------------------------------------------
# GPIO  — asserts the physical wiring, not just command acceptance
# ---------------------------------------------------------------------------
Verify GPIO Input Reads Low When Tied To GND
    [Documentation]    PB2 is wired to GND and must read 0.
    ${resp}=    Read Gpio    B    2
    Should Be Equal As Strings    ${resp}    GPIO VALUE: 0

Verify GPIO Input Reads High When Tied To 3V3
    [Documentation]    PB7 is wired to 3.3V and must read 1.
    ${resp}=    Read Gpio    B    7
    Should Be Equal As Strings    ${resp}    GPIO VALUE: 1

Verify GPIO Output Drive High
    [Documentation]    Drive the on-board LED pin (PC13) high; assert acknowledgement.
    ${resp}=    Write Gpio    C    13    1
    Should Start With    ${resp}    Queued:

Verify GPIO Output Drive Low
    [Documentation]    Drive PC13 low; assert acknowledgement.
    ${resp}=    Write Gpio    C    13    0
    Should Start With    ${resp}    Queued:

# ---------------------------------------------------------------------------
# PWM  — 4 independent timers: ch1=PA8, ch2=PA15, ch3=PB8, ch4=PB6
# ---------------------------------------------------------------------------
Verify PWM Channel 1 Configuration
    [Documentation]    Set PWM channel 1 to 1 kHz at 50% duty (500 permille).
    ${resp}=    Set Pwm Duty Cycle    1    1000    500
    Should Start With    ${resp}    Queued:

Verify PWM Channel 3 Configuration
    [Documentation]    Set PWM channel 3 (now TIM10/PB8) to 2 kHz at 25% duty.
    ${resp}=    Set Pwm Duty Cycle    3    2000    250
    Should Start With    ${resp}    Queued:

Verify PWM Rejects Invalid Channel
    [Documentation]    Channel 5 is out of range (1-4) and must be rejected.
    ${resp}=    Set Pwm Duty Cycle    5    1000    500
    Should Contain    ${resp}    Invalid channel

# ---------------------------------------------------------------------------
# SPI  — command-acceptance level (loopback data assertion needs the async
#        log path; see SPI Loopback keyword note below)
# ---------------------------------------------------------------------------
Verify SPI1 Master Write Accepted
    [Documentation]    Queue a 2-byte SPI master write on channel 1.
    ${resp}=    Spi Write    1    A5F0
    Should Start With    ${resp}    Queued:

Verify SPI2 Slave Arm Accepted
    [Documentation]    Arm SPI2 as slave for a short window; assert acknowledgement.
    ${resp}=    Spi Slave Wait    2    500    DEADBEEF
    Should Start With    ${resp}    Queued:

# ---------------------------------------------------------------------------
# UART  — USART1 (PA9/PA10) <-> USART2 (PA2/PA3) crossover loopback.
#         RX is captured continuously into a ring buffer, so send-then-recv
#         across separate shell commands reliably observes the bytes.
# ---------------------------------------------------------------------------
Verify UART1 To UART2 Loopback
    [Documentation]    Send "HELLO" out USART1 (PA9 TX); assert it arrives on USART2 RX (PA3).
    Uart Send Data    1    HELLO
    ${resp}=    Uart Receive Data    2    5    1000
    Should Be Equal As Strings    ${resp}    UART DATA: HELLO

Verify UART2 To UART1 Loopback
    [Documentation]    Send "WORLD" out USART2 (PA2 TX); assert it arrives on USART1 RX (PA10).
    Uart Send Data    2    WORLD
    ${resp}=    Uart Receive Data    1    5    1000
    Should Be Equal As Strings    ${resp}    UART DATA: WORLD

Verify UART Rejects Invalid Channel
    [Documentation]    Channel 3 is out of range (1-2) and must be rejected.
    ${resp}=    Uart Send Data    3    HELLO
    Should Contain    ${resp}    Invalid UART channel
