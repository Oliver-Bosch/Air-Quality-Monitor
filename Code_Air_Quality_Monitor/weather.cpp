#include "weather.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// === Globale Wetterdaten ===
WeatherAktuell wetterAktuell;
WeatherStunde  wetterStunden[WEATHER_HOURLY_LEN];
float wetterTagMin = 0.0f, wetterTagMax = 0.0f;

static unsigned long letzterWetterAbruf = 0;
static const unsigned long WETTER_INTERVALL = 5UL * 60UL * 1000UL;  // 5 Minuten

static void fetchWetter() {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(WEATHER_LAT, 4) +
                 "&longitude=" + String(WEATHER_LON, 4) +
                 "&current=temperature_2m,weathercode,precipitation" +
                 "&hourly=temperature_2m,weathercode,precipitation" +
                 "&daily=temperature_2m_max,temperature_2m_min" +
                 "&forecast_days=2&timezone=auto";

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == 200) {
        String payload = http.getString();
        Serial.println(payload);

        JsonDocument doc; 
        DeserializationError err = deserializeJson(doc, payload);

        if (!err) {
            wetterAktuell.temp        = doc["current"]["temperature_2m"] | 0.0f;
            wetterAktuell.weathercode = doc["current"]["weathercode"] | 0;
            wetterAktuell.regen       = doc["current"]["precipitation"] | 0.0f;
            wetterAktuell.gueltig     = true;

            wetterTagMax = doc["daily"]["temperature_2m_max"][0] | 0.0f;
            wetterTagMin = doc["daily"]["temperature_2m_min"][0] | 0.0f;

            // ZUVERLÄSSIGER FIX: Wir holen uns die aktuelle Stunde direkt von der Wetter-API!
            // Format in der API ist z.B. "2023-10-10T14:00"
            String apiTime = doc["current"]["time"] | "";
            int aktuelleStunde = 0;
            
            if (apiTime.length() >= 13) {
                // Schneidet die Stunden aus dem String (z.B. "14")
                aktuelleStunde = apiTime.substring(11, 13).toInt(); 
            } else {
                // Fallback, falls die API mal kein Time-Feld schickt
                time_t now = time(nullptr);
                struct tm* t = localtime(&now);
                if (t != nullptr) aktuelleStunde = t->tm_hour;
            }

            // Schleife füllt die nächsten Stunden ab.
            for (int i = 0; i < WEATHER_HOURLY_LEN; i++) {
                int idx = aktuelleStunde + 1 + i;   // ab der nächsten vollen Stunde
                if (idx > 47) idx = 47;             // forecast_days=2 -> max. 48 Stunden (Index 0-47)
                
                wetterStunden[i].stunde      = idx % 24; // % 24 sorgt dafür, dass nach 23 Uhr wieder 0 Uhr kommt
                wetterStunden[i].temp        = doc["hourly"]["temperature_2m"][idx] | 0.0f;
                wetterStunden[i].weathercode = doc["hourly"]["weathercode"][idx] | 0;
                wetterStunden[i].regen       = doc["hourly"]["precipitation"][idx] | 0.0f;
            }

            // DEBUG: prüfen, ob "precipitation" überhaupt aus dem JSON kommt.
            // Zeigt an, ob im Rohpayload oben ein "hourly":{"precipitation":[...]}-Array
            // mit von 0 verschiedenen Werten existiert und ob idx korrekt hineinzeigt.
            Serial.printf("DEBUG regen JETZT: %.2f mm\n", wetterAktuell.regen);
            for (int i = 0; i < WEATHER_HOURLY_LEN; i++) {
                Serial.printf("DEBUG regen[%d] (Stunde %02d): %.2f mm\n",
                              i, wetterStunden[i].stunde, wetterStunden[i].regen);
            }
        }
    }

    http.end();
}

void updateWetter() {
    if (letzterWetterAbruf == 0 || millis() - letzterWetterAbruf > WETTER_INTERVALL) {
        letzterWetterAbruf = millis();
        fetchWetter();
    }
}

// WMO-Weathercodes gemäß Open-Meteo-Dokumentation
const char* wetterSymbolText(int code) {
    if (code == 0)                       return "Klar";
    if (code == 1 || code == 2)          return "Wolkig";
    if (code == 3)                       return "Bedeckt";
    if (code == 45 || code == 48)        return "Nebel";
    if (code >= 51 && code <= 57)        return "Niesel";
    if (code >= 61 && code <= 67)        return "Regen";
    if (code >= 71 && code <= 77)        return "Schnee";
    if (code >= 80 && code <= 82)        return "Schauer";
    if (code >= 85 && code <= 86)        return "Schneeschauer";
    if (code >= 95)                      return "Gewitter";
    return "?";
}

uint16_t wetterFarbeFuerCode(int code, uint16_t cGreen, uint16_t cYellow, uint16_t cRed, uint16_t cCyan) {
    if (code == 0 || code == 1 || code == 2) return cGreen;
    if (code >= 95)                          return cRed;
    if (code == 3 || code == 45 || code == 48) return cYellow;
    return cCyan;  // Nieselregen, Regen, Schnee, Schauer
}