#pragma once

#include <Arduino.h>
#include "daten.h"

// ==========================================
// MATHEMATIK & ARRAY HILFSFUNKTIONEN
// ==========================================
void addValue(float* arr, int len, float val);
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max);
float calcPressureAMSL(float pressure, float temperature, float altitude);
void updatePressureTrend(float pressureAMSL);

// ==========================================
// Air Quality Calculation
// ==========================================

float calcAbsHumidity(float tempC, float relHum);
float calcDewPoint(float tempC, float relHum);
float heatIndex(float tempC, float rh);
int luftqualitaetsIndex(float co2, float tvoc);
int lueEmpfehlung(float co2, float tvoc, float innenAbs, float aussenAbs);

// ==========================================
// HARDWARE 
// ==========================================

void updateLEDs(const SensorDaten &daten);





