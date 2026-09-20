#include "utils.h"
#include "daten.h"
#include "display.h"
#include "config.h"
#include "espnow.h"
#include "wifi_data.h"
#include "sensor.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <math.h>  
#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include <NTPClient.h>


// ==========================================
// MATHEMATIK & ARRAY HILFSFUNKTIONEN
// ==========================================

void addValue(float* arr, int len, float val) {
    for (int i = 0; i < len - 1; i++) arr[i] = arr[i + 1];
    arr[len - 1] = val;
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
    if (x < in_min) x = in_min;
    if (x > in_max) x = in_max;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float calcPressureAMSL(float pressure, float temperature, float altitude){
    return pressure / pow(1.0f - (0.0065f * altitude) / (temperature + 273.15f + 0.0065f * altitude), 5.257f);
}



// ==========================================
// Air Quality Calculation
// ==========================================

// Absolute Luftfeuchte in g/m³ (Magnus-Näherung)
float calcAbsHumidity(float tempC, float relHum) {
    return (6.112f * exp((17.67f * tempC) / (tempC + 243.5f)) * relHum * 2.1674f)
           / (273.15f + tempC);
}

// Taupunkt in °C (Magnus-Formel)
float calcDewPoint(float tempC, float relHum) {
    if (relHum <= 0.0f) relHum = 0.01f;
    float a = 17.67f, b = 243.5f;
    float gamma = log(relHum / 100.0f) + (a * tempC) / (b + tempC);
    return (b * gamma) / (a - gamma);
}

// Heat Index in °C (Steadman/Rothfusz, gültig ab ~27°C / RH>40%)
float heatIndex(float tempC, float rh) {
    if (tempC < 27.0f || rh < 40.0f) return tempC;
    float T = tempC * 9.0f / 5.0f + 32.0f;
    float HI = -42.379f
             +  2.04901523f * T
             + 10.14333127f * rh
             -  0.22475541f * T * rh
             -  0.00683783f * T * T
             -  0.05481717f * rh * rh
             +  0.00122874f * T * T * rh
             +  0.00085282f * T * rh * rh
             -  0.00000199f * T * T * rh * rh;
    return (HI - 32.0f) * 5.0f / 9.0f;
}

// [NEU] Luftqualitätsindex 0–5 aus CO2 + TVOC
// 0=Sehr gut, 1=Gut, 2=Mäßig, 3=Schlecht, 4=Sehr schlecht, 5=Gefährlich
int luftqualitaetsIndex(float co2, float tvoc) {
    int co2Stufe = 0;
    if      (co2 >= 2000) co2Stufe = 5;
    else if (co2 >= 1500) co2Stufe = 4;
    else if (co2 >= 1200) co2Stufe = 3;
    else if (co2 >= 1000) co2Stufe = 2;
    else if (co2 >= 800)  co2Stufe = 1;

    int tvocStufe = 0;
    if      (tvoc >= 2200) tvocStufe = 5;
    else if (tvoc >= 1430) tvocStufe = 4;
    else if (tvoc >= 660)  tvocStufe = 3;
    else if (tvoc >= 220)  tvocStufe = 2;
    else if (tvoc >= 65)   tvocStufe = 1;

    return max(co2Stufe, tvocStufe);
}

// [NEU] Lüftungsempfehlung:
// 0=Nicht nötig (grün), 1=Empfohlen (gelb), 2=Dringend (rot)
int lueEmpfehlung(float co2, float tvoc, float innenAbs, float aussenAbs) {
    int aqi = luftqualitaetsIndex(co2, tvoc);
    bool aussenBesser = (aussenAbs < innenAbs * 0.95f) || (innenAbs > 12.0f);
    if (aqi >= 3) return 2; 
    if (aqi >= 1) return aussenBesser ? 1 : 0;
    return 0;
}


// ==========================================
// Hardware
// ==========================================

void updateLEDs(const SensorDaten &daten) {
    if (!LED_ALERTS) return;

    if (daten.co2 >= CO2_th_critical) {
        digitalWrite(CO2_CR_LED, HIGH);
        digitalWrite(CO2_AL_LED, LOW);
    } else if (daten.co2 >= CO2_th_alert) {
        digitalWrite(CO2_AL_LED, HIGH);
        digitalWrite(CO2_CR_LED, LOW);
    } else {
        digitalWrite(CO2_AL_LED, LOW);
        digitalWrite(CO2_CR_LED, LOW);
    }
    digitalWrite(RH_LED, daten.humSht >= RH_th_critical ? HIGH : LOW);
}

