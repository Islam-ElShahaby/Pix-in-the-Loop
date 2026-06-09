# HIL Simulator 🎮
* HIL or *Hardware in the Loop* is the joker, flashed with code to listen to commands from an orchestrator node and execute the corresponding behaviours to test a specific functionality and respond back with results to the orchestrator.
* It runs under Zephyr for our purposes, and exposes several features validation among them are GPIOs, ADC, PWM, SPI, and UART.
* The means of communication btwn HIL and the orchestrator is serial communication, again for our purposes.
