#pragma once
#include <stdint.h>

struct SensorDaten {
    uint16_t co2 = 0;
    float tempSht = 0.0;
    float humSht = 0.0;
    float tempScd = 0.0;
    float humScd = 0.0;
    float dewPoint = 0.0;
    float absHumidity = 0.0;
    float pm10 = 0.0;  // PM 1.0
    float pm25 = 0.0;  // PM 2.5
    float pm100 = 0.0; // PM 10.0
    uint16_t tvoc = 0; // TVOC in ppb
    float lux = 0.0; 
};