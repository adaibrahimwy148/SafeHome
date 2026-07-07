# SafeHome 🏠🔒

SafeHome is an Arduino-based smart home security system designed to protect houses against unauthorized access, fire, and smoke hazards.

The system combines multiple sensors, an LCD display, an IR remote controller, and Telegram communication to provide both local and remote monitoring and control.

## Features

### 🔥 Fire and Smoke Protection
- Detects dangerous environmental conditions using fire and smoke sensors.
- Provides immediate alerts when a potential hazard is detected.
- Activates water pump and opens doors and windows in case of fires.
- Activates fan and opens windows in case of smoke.
- Sends notifications to the homeowner through Telegram.

### 🚨 Intrusion Detection
- Detects unauthorized access attempts using ultrasonic sensors.
- Activates alarms and sends instant Telegram notifications.
- Closes doors and windows.

### 🎮 Dual Operating Modes

SafeHome supports two operating modes:

**Automatic Mode**
- The system monitors all sensors continuously.
- Actions are triggered automatically when a threat is detected.

**Manual Mode**
- Allows the user to manually control system components.
- Controlled locally using an IR remote controller.

### 📟 LCD Monitoring
- Displays real-time information from sensors.
- Shows system status and detected conditions.

### 📱 Telegram Integration
- Sends alerts directly to the homeowner.
- Allows remote monitoring and control of system components.
- Users can activate or deactivate features even when away from home.

## Hardware Components

- Arduino microcontroller
- LCD display
- IR receiver and remote controller
- Fire/flame sensor
- Smoke/gas sensor
- Ultrasonic sensor
- DHT11 sensor
- Keypad
- Relay
- DC motor (fan)
- Buzzer and LEDs
- Servo motors
- 12V battery supply
- Water pump

## Software and Technologies

- Arduino IDE
- C/C++ programming language
- Telegram Bot API

## System Overview

SafeHome provides an affordable smart security solution by combining environmental monitoring, intrusion detection, and remote communication.

The system continuously reads sensor values, displays information locally, alerts the homeowner when a threat is detected, and allows remote control through Telegram.

## Future Improvements

Possible improvements include:

- Adding a mobile application
- Adding a camera module for visual monitoring
- Cloud data storage
- More advanced automation features
