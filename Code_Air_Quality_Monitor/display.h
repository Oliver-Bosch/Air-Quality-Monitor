#pragma once
#include <Arduino.h>

#include "daten.h"
#include "espnow.h"

void initDisplay();
void zeichneDashboardRaster();
void updateTopBar(const char* datumStr, const char* zeitStr, bool wlanVerbunden);
void updateSensorKacheln(const SensorDaten &daten);
void updateUntererBereich(const SensorDaten &daten);
void displaySchlafen();
void displayAufwachen();
