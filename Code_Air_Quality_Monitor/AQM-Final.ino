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
#include "utils.h"
#include "weather.h"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200); // UTC+2

SensorDaten aktuelleDaten;

static unsigned long letzteZeitSync = 0;


// ==========================================
// Zeit / Datum formatieren
// ==========================================
void formatTimeAndDate(char* timeBuf, char* dateBuf) {
    unsigned long epoch = timeClient.getEpochTime();

    int h = (epoch % 86400L) / 3600;
    int m = (epoch % 3600) / 60;

    sprintf(timeBuf, "%02d:%02d", h, m);

    const char* daysDE[] = {
        "So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"
    };

    const char* daysEN[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };

    int wd = timeClient.getDay();

    const char* dayStr =
        strcmp(LANGUAGE, "GER") == 0
        ? daysDE[wd]
        : daysEN[wd];


    // Datum berechnen
    unsigned long days = epoch / 86400L;

    int year = 1970;

    while (true) {
        bool leap =
            (year % 4 == 0 && year % 100 != 0) ||
            (year % 400 == 0);

        unsigned int diy = leap ? 366 : 365;

        if (days < diy)
            break;

        days -= diy;
        year++;
    }

    int monthDays[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if ((year % 4 == 0 && year % 100 != 0) ||
        (year % 400 == 0)) {
        monthDays[1] = 29;
    }

    int month = 0;

    while (days >= (unsigned long)monthDays[month]) {
        days -= monthDays[month];
        month++;
    }

    sprintf(
        dateBuf,
        "%s %02d.%02d.",
        dayStr,
        (int)days + 1,
        month + 1
    );
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
// NTP synchronisieren
// WLAN bleibt verbunden!
// ==========================================
bool syncZeit() {

    Serial.println("NTP Zeit-Synchronisation...");

    // WLAN sicherstellen
    if (!verbindeWLAN()) {
        Serial.println("NTP abgebrochen: Kein WLAN.");
        return false;
    }

    // NTP initialisieren
    if (!timeClient.isTimeSet()) {
        timeClient.begin();
    }

    // Zeit aktualisieren
    int ntpRetries = 0;

    while (!timeClient.forceUpdate() &&
           ntpRetries < 10) {

        delay(500);

        Serial.print("t");
        ntpRetries++;
    }

    if (ntpRetries < 10) {

        Serial.println();
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

    Serial.println();
    Serial.println("Fehler: NTP Server hat nicht geantwortet.");

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