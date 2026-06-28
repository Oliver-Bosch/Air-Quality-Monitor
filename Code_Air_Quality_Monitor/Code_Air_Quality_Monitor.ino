#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// Eigene Module
#include "config.h"
#include "wifi_data.h"
#include "daten.h"
#include "sensor.h"
#include "display.h"
#include "espnow.h"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200); // UTC+2
SensorDaten aktuelleDaten;

static unsigned long letzteZeitSync = 0;

// ==========================================
// HILFSFUNKTIONEN
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

void formatTimeAndDate(char* timeBuf, char* dateBuf) {
    unsigned long epoch = timeClient.getEpochTime();

    int h = (epoch % 86400L) / 3600;
    int m = (epoch % 3600) / 60;
    sprintf(timeBuf, "%02d:%02d", h, m);

    const char* daysDE[] = {"So","Mo","Di","Mi","Do","Fr","Sa"};
    const char* daysEN[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    int wd = timeClient.getDay();
    const char* dayStr = strcmp(LANGUAGE, "GER") == 0 ? daysDE[wd] : daysEN[wd];

    unsigned long days = epoch / 86400L;
    int year = 1970;
    while (true) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        unsigned int diy = leap ? 366 : 365;
        if (days < diy) break;
        days -= diy;
        year++;
    }
    int monthDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) monthDays[1] = 29;
    int month = 0;
    while (days >= (unsigned long)monthDays[month]) { days -= monthDays[month]; month++; }
    sprintf(dateBuf, "%s %02d.%02d.", dayStr, (int)days + 1, month + 1);
}

// NTP sync: kurz verbinden, Zeit holen, wieder trennen
void syncZeit() {
    Serial.println("NTP Sync...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        timeClient.begin();
        timeClient.update();
        Serial.println("\nZeit synchronisiert");
    } else {
        Serial.println("\nWLAN nicht erreichbar");
    }
    WiFi.disconnect();
    delay(100);
    // ESP-NOW nach WiFi-Disconnect neu initialisieren
    initESPNow();
    letzteZeitSync = millis();
}

// ==========================================
// SETUP
// ==========================================
void setup() {
    Serial.begin(115200);

    // MAC Adresse ausgeben (vor allem anderen)
    WiFi.mode(WIFI_STA);
    delay(100);
    Serial.print("MAC Adresse: ");
    Serial.println(WiFi.macAddress());

    // LEDs & Backlight init
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, LOW);
    pinMode(CO2_AL_LED, OUTPUT); digitalWrite(CO2_AL_LED, LOW);
    pinMode(CO2_CR_LED, OUTPUT); digitalWrite(CO2_CR_LED, LOW);
    pinMode(RH_LED,     OUTPUT); digitalWrite(RH_LED,     LOW);

    // Display & Sensoren starten
    initDisplay();
    zeichneDashboardRaster();
    initSensoren();

    // Zeit holen und danach WiFi trennen
    syncZeit();
}

// ==========================================
// LOOP
// ==========================================
void loop() {
    // Alle 6 Stunden Zeit neu synchronisieren
    if (millis() - letzteZeitSync > 21600000UL) {
        syncZeit();
    }

    static unsigned long letztesUpdate = 0;

    if (millis() - letztesUpdate > UPDATE_TIME) {
        letztesUpdate = millis();

        leseSensoren(aktuelleDaten);

        bool istDunkel = (aktuelleDaten.lux > LUX_TH_DARK && aktuelleDaten.co2 > 0);
        digitalWrite(TFT_BL_PIN, istDunkel ? HIGH : LOW);
        if (istDunkel) {
            digitalWrite(CO2_AL_LED, LOW);
            digitalWrite(CO2_CR_LED, LOW);
            digitalWrite(RH_LED, LOW);
        }

        char timeBuf[10]; char dateBuf[14];
        formatTimeAndDate(timeBuf, dateBuf);
        updateTopBar(dateBuf, timeBuf, false, outdoorVerbunden()); // kein dauerhaftes WLAN mehr
        updateSensorKacheln(aktuelleDaten);
        updateUntererBereich(aktuelleDaten);
        if (!istDunkel) updateLEDs(aktuelleDaten);
    }
    delay(10);
}
