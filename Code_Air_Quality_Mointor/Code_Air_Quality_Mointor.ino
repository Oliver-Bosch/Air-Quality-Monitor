#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUDP.h>
#include <SensirionI2cScd4x.h>
#include <SensirionI2cSht4x.h>
#include <Adafruit_SSD1306.h>

// WLAN
#define WIFI_SSID "X"
#define WIFI_PASS "X" 


// Display
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240


// Pins
#define CO2_LED D5
#define RH_LED D6


// Treshold

#define CO2_th_alert 1000
#define CO2_th_critical 1500
#define RH_th 60

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200); // UTC+2 (Deutschland)



void setup() {
    Serial.begin(115200);
    Wire.begin(D2, D1);
    
    pinMode(CO2_LED, OUTPUT);
    digitalWrite(CO2_LED, LOW);

    pinMode(RH_LED, OUTPUT);
    digitalWrite(RH_LED, LOW);


}

void loop() {
  // put your main code here, to run repeatedly:

}
