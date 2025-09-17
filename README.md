# IoT-Based-Smart-Medication-Reminder-Device

This repository presents an ESP32-powered Smart Medication Reminder, designed to help users stay on track with their medication schedules while also ensuring safe storage conditions. The project was built as part of the EN2853 – Embedded Systems and Applications module.

📌 Project Overview

The system was developed in two stages:

Stage 1 – Core Prototype (Simulation): A functional prototype was created on the Wokwi platform, implementing time synchronization, alarm scheduling, and basic environmental monitoring.

Stage 2 – IoT Integration & Enhancement: The design was extended with IoT features such as real-time monitoring through a Node-RED dashboard, plus automated control to protect light-sensitive medicines.

⚙️ Key Features
🕒 Smart Reminders

Multiple, customizable alarms with easy navigation using push-buttons.

Automatic NTP-based time synchronization with support for timezone adjustments.

Multi-sensory alerts combining buzzer, LED, and OLED display messages.

Simple snooze/stop functionality for alarms.

🌡️ Environment Monitoring & Safety

Continuous temperature and humidity tracking via a DHT sensor.

Alerts when conditions exceed safe thresholds (Temperature: 24–32°C, Humidity: 65–80%).

Light detection using an LDR to safeguard sensitive medicines.

Servo-controlled shading system that dynamically regulates light exposure based on conditions.

🌍 IoT Dashboard Integration

Real-time monitoring and visualization through a Node-RED dashboard.

Remote adjustment of sensor intervals, shading parameters, and temperature targets.

MQTT-based communication using the HiveMQ public broker for seamless data exchange.

🏗️ System Architecture

ESP32 microcontroller handles all sensors, actuators, and IoT communication.

Data is exchanged via MQTT topics (sensor readings → broker → dashboard).

Remote configurations are applied instantly by subscribing to control topics.

Architecture Flow:
ESP32 + Sensors/Actuators ⇄ HiveMQ MQTT Broker ⇄ Node-RED Dashboard

🛠️ Tech Stack

Hardware

ESP32 Microcontroller

DHT22 Temperature & Humidity Sensor

Light Dependent Resistor (LDR)

Servo Motor

Buzzer + LED Indicators

SSD1306 OLED Display

Push Button Interface

Software & Tools

Arduino (C/C++) for embedded programming

Wokwi for simulation and prototyping

MQTT for IoT communication

HiveMQ Public Broker

Node-RED for dashboard visualization and control
