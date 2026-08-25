# ESP32 Laser Tag
Arduino program for real life laser-tag system using the ESP32 micro controller.
Built as a home project.

## Installation
Clone or download code.

Uses the libraries:
- IRremoteESP8266
- Adafruit GFX
- Adafruit SDD1306

Push to ESP32. May require using BOOT on the ESP32.

## Hardware
- Runs on ESP-wroom-32 micro controller
- Infrared digital firing LED on PWM pin. Pulses with frequency when firing
- Digital IR receiver. May require multi-receiver array for more reliable detection. Receives incoming IR shots
- Hit digital LED, red recommended. Flashes when hit or killed
- 128x64 OLED Display. Displays gun selection and GUI with health and ammunition
- Muzzle flash digital LED. Flashes when shooting
- Passive Piezo buzzer. Buzzes when firing, hit and for menu navigation confirmation
- Fire button. Selects in menus, and fires the gun
- Reload button. Cycles in menus, and reloads the gun
