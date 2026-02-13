---
name: esp-idf
description: Espressif IoT Development Framework 
---


# ESP-IDF 101

The latest esp-idf major version will be v5.x, and make sure you using the latest I2C/ADC API.

Always capture the state that caused the interrupt at the moment the interrupt fires (inside the ISR handler), not later when the task runs.


# Button
For any single press button application, you must do the deboucing inside the isr. 

# ISR
The general rule: keep ISRs short and non-blocking. Defer lengthy or blocking work to regular task context.