#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

// Eigene Module
#include "config.h"
#include "wifi_data.h"
#include "daten.h"
#include "sensor.h"
#include "display.h"
#include "espnow.h"
#include "utils.h"
#include "weather.h"

// Zeitzone: Mitteleuropa mit automatischer Sommer-/Winterzeit
// (Sommerzeit: letzter Sonntag im Maerz bis letzter Sonntag im Oktober)
// Kann alternativ in der config.h definiert werden.
#ifndef TIMEZONE
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"
#endif

SensorDaten aktuelleDaten;

static unsigned long letzteZeitSync = 0;


// ==========================================
// Zeit / Datum formatieren
// ==========================================
void formatTimeAndDate(char* timeBuf, char* dateBuf) {
    struct tm t;

    // Zeit noch nicht synchronisiert
    if (!getLocalTime(&t, 0)) {
        strcpy(timeBuf, "--:--");
        strcpy(dateBuf, "--.--.");
        return;
    }

    sprintf(timeBuf, "%02d:%02d", t.tm_hour, t.tm_min);

    const char* daysDE[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
    const char* daysEN[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    const char* dayStr =
        strcmp(LANGUAGE, "GER") == 0
        ? daysDE[t.tm_wday]
        : daysEN[t.tm_wday];

    sprintf(dateBuf, "%s %02d.%02d.", dayStr, t.tm_mday, t.tm_mon + 1);
}


// ==========================================
// WLAN verbinden
// ==========================================
bool verbindeWLAN() {

    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    Serial.print("Verbinde mit WLAN");

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int attempts = 0;

    while (WiFi.status() != WL_CONNECTED &&
           attempts < 30) {

        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {

        Serial.println();
        Serial.println("WLAN verbunden!");

        Serial.print("IP-Adresse: ");
        Serial.println(WiFi.localIP());

        Serial.print("RSSI: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");

        return true;
    }

    Serial.println();
    Serial.println("Fehler: WLAN nicht erreichbar.");

    return false;
}


// ==========================================
// NTP synchronisieren (inkl. Zeitzone und Sommerzeit)
// WLAN bleibt verbunden!
// ==========================================
bool syncZeit() {

    Serial.println("NTP Zeit-Synchronisation...");

    // WLAN sicherstellen
    if (!verbindeWLAN()) {
        Serial.println("NTP abgebrochen: Kein WLAN.");
        return false;
    }

    // Zeitzone setzen und per NTP synchronisieren
    configTzTime(TIMEZONE, "pool.ntp.org");

    struct tm t;

    if (getLocalTime(&t, 10000)) {

        Serial.println("Zeit erfolgreich synchronisiert!");

        char timeBuf[10];
        char dateBuf[14];

        formatTimeAndDate(timeBuf, dateBuf);

        Serial.print("Datum: ");
        Serial.println(dateBuf);

        Serial.print("Zeit:  ");
        Serial.println(timeBuf);

        letzteZeitSync = millis();

        return true;
    }

    Serial.println("Fehler: NTP Server hat nicht geantwortet.");

    // Nicht sofort erneut versuchen, sondern in 1 Minute
    letzteZeitSync = millis() - 21600000UL + 60000UL;

    return false;
}


// ==========================================
// SETUP
// ==========================================
void setup() {

    Serial.begin(115200);

    delay(100);

    // ======================================
    // WLAN
    // ======================================

    WiFi.mode(WIFI_STA);

    delay(100);

    Serial.print("MAC Adresse: ");
    Serial.println(WiFi.macAddress());

    // WLAN dauerhaft verbinden
    if (verbindeWLAN()) {

        // NTP direkt synchronisieren
        syncZeit();

    } else {

        Serial.println(
            "WARNUNG: Start ohne WLAN."
        );
    }


    // ======================================
    // LEDs & Backlight
    // ======================================

    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, LOW);


    // ======================================
    // Display
    // ======================================

    initDisplay();

    zeichneDashboardRaster();

    Serial.println("Display init");


    // ======================================
    // Sensoren
    // ======================================

    initSensoren();


    // ======================================
    // WLAN bleibt jetzt aktiv!
    // ======================================
}


// ==========================================
// LOOP
// ==========================================
void loop() {

    // ======================================
    // WLAN überwachen
    // ======================================

    if (WiFi.status() != WL_CONNECTED) {

        Serial.println("WLAN Verbindung verloren!");

        verbindeWLAN();
    }


    // ======================================
    // Alle 6 Stunden NTP synchronisieren
    // ======================================

    if (millis() - letzteZeitSync > 21600000UL) {

        syncZeit();
    }


    // ======================================
    // Sensor / Display Update
    // ======================================

    static unsigned long letztesUpdate = 0;

    if (millis() - letztesUpdate > UPDATE_TIME) {

        letztesUpdate = millis();


        // Sensoren lesen
        leseSensoren(aktuelleDaten);


        // ==================================
        // Display Beleuchtung
        // ==================================

        bool istDunkel =
            (aktuelleDaten.lux > LUX_TH_DARK &&
             aktuelleDaten.co2 > 0);

        digitalWrite(
            TFT_BL_PIN,
            istDunkel ? HIGH : LOW
        );


        if (istDunkel) {

            digitalWrite(CO2_AL_LED, LOW);
            digitalWrite(CO2_CR_LED, LOW);
            digitalWrite(RH_LED, LOW);
        }


        // ==================================
        // Uhrzeit / Datum
        // ==================================

        char timeBuf[10];
        char dateBuf[14];

        formatTimeAndDate(
            timeBuf,
            dateBuf
        );


        // ==================================
        // Top Bar
        // WLAN bleibt dauerhaft aktiv
        // ==================================

        updateTopBar(
            dateBuf,
            timeBuf,
            WiFi.status() == WL_CONNECTED
        );


        // ==================================
        // Sensordaten
        // ==================================

        updateSensorKacheln(
            aktuelleDaten
        );


        updateUntererBereich(
            aktuelleDaten
        );


        // ==================================
        // LEDs
        // ==================================

        if (!istDunkel) {

            updateLEDs(
                aktuelleDaten
            );
        }
    }


    delay(10);
}
