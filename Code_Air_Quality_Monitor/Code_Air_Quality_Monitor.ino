#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUDP.h>
#include <SensirionI2cScd4x.h>
#include <SensirionI2cSht4x.h>
#include <Adafruit_SSD1306.h>
#include <cmath>

// Settings
#define LED_ALERTS True // Toggles the CO_2 & RH alert LEDs | True: On
#define Oriantation 1 // 0: Vertical; 1: Horizontal
#define UpdateTime 500 // Delay between each refresh/measurement in ms

// WLAN
#define WIFI_SSID "X"
#define WIFI_PASS "X" 


// Display
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240


// Pins
#define CO2_al_LED D5
#define CO2_cr_LED D6
#define RH_LED D7


// Tresholds (CO_2: ppm, RH: %, pmX: µg/m³, tvoc: µg/m³)
#define CO2_th_alert 1000 
#define CO2_th_critical 1500

#define RH_th 60

#define pm25_th_alert 15
#define pm25_th_critical 25

#define pm10_th_alert 20 
#define pm10_th_critical 50

#define tvoc_th_alert 300
#define tvoc_th_critical 600


// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200); // UTC+2 (Deutschland)


double calculateDewPoint(double temperatureC, double relativeHumidity) {
    
    // Temperature in °C
    // Relativ humidity in % 
    // Dew point in °C

    constexpr double a = 17.62;
    constexpr double b = 243.12; // °C

    double gamma = std::log(relativeHumidity / 100.0) +
                   (a * temperatureC) / (b + temperatureC);

    double dewPoint = (b * gamma) / (a - gamma);

    return dewPoint;
}

double calculateAbsoluteHumidity(double temperatureC, double relativeHumidity)
{
    // Saturation vapour pressure nach Magnus (hPa)
    double saturationVaporPressure = 6.112 * std::exp((17.67 * temperatureC) / (temperatureC + 243.5));

    // Actual saturtion vapour pressure (hPa)
    double vaporPressure = saturationVaporPressure * (relativeHumidity / 100.0);

    // Absolute humidity (g/m³)
    double absoluteHumidity = 216.7 * vaporPressure / (temperatureC + 273.15);

    return absoluteHumidity;
}



void setup() {
    Serial.begin(115200);
    Wire.begin(D2, D1);
    
    pinMode(CO2_cr_LED, OUTPUT);
    digitalWrite(CO2_cr_LED, LOW);

    pinMode(CO2_al_LED, OUTPUT);
    digitalWrite(CO2_al_LED, LOW);

    pinMode(RH_LED, OUTPUT);
    digitalWrite(RH_LED, LOW);
}

void loop() {
  // put your main code here, to run repeatedly:











    if(LED_ALERTS){

        if (co2 >= CO2_th_alert && co2 < CO2_th_critical) {
            digitalWrite(CO2_al_LED, HIGH);
        } else if(co2 >= CO2_th_critical) {
            digitalWrite(CO2_cr_LED, HIGH);
        } else {
            digitalWrite(CO2_al_LED, LOW);
            digitalWrite(CO2_cr_LED, LOW);
        }     

        if (humSht >= RH_th) {
            digitalWrite(RH_LED, HIGH);
        } else {
            digitalWrite(RH_LED, LOW);
            }

    }



}
