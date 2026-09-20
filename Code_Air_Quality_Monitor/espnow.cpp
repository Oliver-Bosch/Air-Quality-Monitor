#include "espnow.h"
#include "utils.h"
#include "config.h"

#include <WiFi.h>
#include <esp_now.h>

typedef struct {
    float temp;
    float hum;
    float pressure;
} OutdoorPaket;

static OutdoorDaten outdoorDaten;

static void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (len != sizeof(OutdoorPaket)) return;
    OutdoorPaket paket;
    memcpy(&paket, data, sizeof(paket));
    outdoorDaten.temp          = paket.temp;
    outdoorDaten.hum           = paket.hum;
    outdoorDaten.pressure      = paket.pressure;
    outdoorDaten.gueltig       = true;
    outdoorDaten.letzterEmpfang = millis();
    Serial.printf("ESP-NOW empfangen: Temp=%.1f°C  Hum=%.1f%%  Druck=%.1fhPa\n",
                  paket.temp, paket.hum, paket.pressure);
    updatePressureTrend(calcPressureAMSL( paket.pressure, paket.temp, h_AMSL));              
}

void initESPNow() {
    WiFi.mode(WIFI_STA);
    delay(100);
    WiFi.disconnect();
    Serial.print("MAC Adresse: ");
    Serial.println(WiFi.macAddress());
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Fehler!");
        return;
    }
    esp_now_register_recv_cb(onDataRecv);
    Serial.println("ESP-NOW Empfänger bereit");
}

OutdoorDaten getOutdoorDaten() {
    return outdoorDaten;
}

bool outdoorVerbunden(unsigned long timeoutMs) {
    if (!outdoorDaten.gueltig) return false;
    return (millis() - outdoorDaten.letzterEmpfang) < timeoutMs;
}