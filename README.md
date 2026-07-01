# Air-Quality-Monitor

This is a guide to build your own Air-Quality-Monitor (**AQM**). The AQM measures CO2, relative humidity, temperature, and particulate matter. The current readings are displayed on a 4-inch TFT screen and continuously logged to an onboard SD card. The device is connected to the local network via Wi-Fi to synchronise time. Additionally, it features three LED indicators that function as visual alarms, which trigger when predefined maximum thresholds for either CO2 or humidity are exceeded. The alerts and display are also controlled via a ambient light sensor to toggle them at night.

When connected with the [AQM-Outdoor-Add-On](https://github.com/Oliver-Bosch/AQM-Outdoor-Add-On) a new tile will appear in the lower bar and give additional informations for the outside temperature, humidity and air pressure. With this information the ventilation logic and weather prediction is feed.


# Table of Contents

[Parts] # Parts
[Assembly] # Assembly
[Installation] # Installation
[Price] # Price
[Notes] # Notes

# Parts

- ESP32S
- SCD41 CO₂ Sensor
- SHT40 Temperature & Humidity Sensor
- SPS30 Particulate Matter Sensor
- VEML7700 Ambient Light Sensor
- ILI9488 4 Inch TFT Display
- 3x $$200 \Omega$$ Resistor
- Red, Blue and Yellow LED



# Assembly

Soldering


# Installation

# Display Layout

The AQM can either be used in a vertical or horizontal configuration. For that you can change 'ORIENTATION' in the config file to either 0 (vertical) or 1 (horizontal).



# Price

<div align="center">

| Part | Price |
|----------|--------|
| ESP32S | 3,42 € |
| SCD41  | 17,99 € |
| SHT40  | 2,79 € |
| SPS30  | 10,45 € |
| VEML7700 | 1,66 € |
| ILI9488 4" TFT Display | 13,52 € |
| 3× 200 Ω Resistor | 0,15 € |
| Red LED | 0,1 € |
| Yellow LED| 0,1€ |
| Blue LED | 0,15 € |
| **Total** | **50.18 €** |

\* All parts sourced on Aliexpress 2026

# Notes

Future Home Assistant integration to get rid of the sd card is planned. 