#pragma once
#include <Arduino.h>

// Anzahl der angezeigten Stunden in der Vorhersage-Leiste
#define WEATHER_HOURLY_LEN 12

struct WeatherAktuell {
    float temp       = 0.0f;
    int   weathercode = 0;
    float regen;
    bool  gueltig     = false;
};

struct WeatherStunde {
    int   stunde      = 0;   // Stunde des Tages (0-23)
    float temp        = 0.0f;
    int   weathercode = 0;
    float regen;
};

extern WeatherAktuell wetterAktuell;
extern WeatherStunde  wetterStunden[WEATHER_HOURLY_LEN];
extern float wetterTagMin, wetterTagMax;

// Ruft intern nur alle WETTER_INTERVALL Millisekunden tatsächlich die API ab,
// kann also gefahrlos in jedem Loop-Durchlauf aufgerufen werden.
void updateWetter();

// WMO-Weathercode (Open-Meteo) -> kurzer deutscher Text
const char* wetterSymbolText(int code);

// WMO-Weathercode -> Farbe passend zum bestehenden Farbschema
uint16_t wetterFarbeFuerCode(int code, uint16_t cGreen, uint16_t cYellow, uint16_t cRed, uint16_t cCyan);
