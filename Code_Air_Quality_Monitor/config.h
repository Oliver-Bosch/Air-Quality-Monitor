#pragma once

// =====================
// Settings
// =====================
#define LANGUAGE        "GER"               // "GER" for german or "ENG" for english
#define LED_ALERTS      true                // CO2 & RH Alert LEDs active
#define ORIENTATION     3                   // 0: Vertikal, 1: Horizontal
#define UPDATE_TIME     500                 // time in ms between measurements
#define h_AMSL          179                 // Height above mean sea level of the Sensor (only used for weather prediction with AQM-Outdoor-Add-On)  
#define OUTDOOR_TIMEOUT_MS 10000            //
#define HA_SYNCH_TIME   60                  // Time between broadcast of the measurements to Home Assistant in seconds

#define WEATHER_LAT 50.786638
#define WEATHER_LON 6.070075
#define WEATHER_HOURLY_LEN 12

// =====================
// Pins (ESP32)
// =====================
#define SDA_PIN         21
#define SCL_PIN         22

#define CO2_AL_LED      25                  // Alert LED (gelb)
#define CO2_CR_LED      26                  // Critical LED (rot)
#define RH_LED          27                  // Feuchte LED

#define TFT_BL_PIN      17                  // Backlight pin of the TFT display

#define SD_CS_PIN       5                   // Pin for writing the SD card


// =====================
// Threshold values
// =====================

#define LUX_TH_DARK        0.3               // For values below this threshold the screen toggles off

#define CO2_th_alert       1000 
#define CO2_th_critical    1500

#define RH_th_alert_low    35.0             // Zu trockene Luft
#define RH_th_critical     60.0             // Zu feuchte Luft (Schimmelgefahr)
            
#define TEMP_th_alert_low  18.0             // Zu kalt
#define TEMP_th_alert_high 25.0             // Zu warm
#define TEMP_th_critical   28.0             // Hitze

#define pm25_th_alert      15
#define pm25_th_critical   25

#define pm10_th_alert      20 
#define pm10_th_critical   50

#define tvoc_th_alert      300
#define tvoc_th_critical   600

// =====================
// Graphs (Scaling for min & max)
// =====================
#define GRAPH_CO2_MIN      400
#define GRAPH_CO2_MAX      2000

#define GRAPH_RH_MIN       20
#define GRAPH_RH_MAX       80

#define GRAPH_TEMP_MIN     10
#define GRAPH_TEMP_MAX     40

#define GRAPH_PM_MIN       0
#define GRAPH_PM_MAX       50

#define GRAPH_TVOC_MIN     0
#define GRAPH_TVOC_MAX     1000