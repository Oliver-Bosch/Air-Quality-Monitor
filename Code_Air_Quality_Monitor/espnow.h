#pragma once
#include <Arduino.h>

struct OutdoorDaten {
    float temp     = 0.0;
    float hum      = 0.0;
    float pressure = 0.0;
    bool  gueltig  = false;
    unsigned long letzterEmpfang = 0; // millis() des letzten Pakets
};

void initESPNow();
OutdoorDaten getOutdoorDaten();
bool outdoorVerbunden(unsigned long timeoutMs = 30000); // true wenn Daten < 30s alt