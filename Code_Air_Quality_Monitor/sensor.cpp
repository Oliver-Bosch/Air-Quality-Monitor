#include "sensor.h"
#include "config.h"
#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include <SensirionI2cSht4x.h>
#include <SensirionI2cSps30.h>
#include <Adafruit_VEML7700.h>
#include <cmath>

#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

// ==========================================
// SENSOR-OBJEKTE
// ==========================================
SensirionI2cScd4x scd4x;
SensirionI2cSht4x sht4x;
SensirionI2cSps30 sps30;
Adafruit_VEML7700 veml = Adafruit_VEML7700();

static bool vemlOK = false;
static bool sps30OK = false;

// ==========================================
// HILFSFUNKTIONEN FÜR BERECHNUNGEN
// ==========================================
float calcDewPoint(float temp, float hum) {
    float a = 17.271;
    float b = 237.7;
    float alpha = ((a * temp) / (b + temp)) + log(hum / 100.0);
    return (b * alpha) / (a - alpha);
}

float calcAbsHumidity(float temp, float hum) {
    return (6.112 * pow(2.71828, (17.67 * temp) / (temp + 243.5)) * hum * 2.1674) / (273.15 + temp);
}

// ==========================================
// INITIALISIERUNG ALLER SENSOREN
// ==========================================
void initSensoren() {
    Wire.begin(SDA_PIN, SCL_PIN);
    delay(100);

    // 1. SCD41 (CO2)
    scd4x.begin(Wire, SCD41_I2C_ADDR_62);
    scd4x.wakeUp();
    scd4x.stopPeriodicMeasurement();
    scd4x.reinit();
    if (scd4x.startPeriodicMeasurement() != 0) {
        Serial.println("SCD41 Start Fehler!");
    } else {
        Serial.println("SCD41 OK");
    }

    // 2. SHT40 (Temp & Feuchte)
    sht4x.begin(Wire, SHT40_I2C_ADDR_44);
    Serial.println("SHT40 OK");

    // 3. SPS30 (Feinstaub)
    sps30.begin(Wire, SPS30_I2C_ADDR_69);
    sps30.stopMeasurement();
    delay(50);
    int16_t err = sps30.startMeasurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_UINT16);
    if (err != NO_ERROR) {
        Serial.println("SPS30 Start Fehler!");
        sps30OK = false;
    } else {
        Serial.println("SPS30 OK");
        sps30OK = true;
    }

    Wire.end();
    delay(50);
    Wire.begin(SDA_PIN, SCL_PIN);
    delay(100);

    vemlOK = veml.begin();
    if (!vemlOK) {
        Serial.println("VEML7700 nicht gefunden - Lux deaktiviert");
    } else {
        veml.setGain(VEML7700_GAIN_1);
        veml.setIntegrationTime(VEML7700_IT_100MS);
        Serial.println("VEML7700 OK");
    }
}

// ==========================================
// SENSOREN AUSLESEN UND INS PAKET PACKEN
// ==========================================

bool leseSensoren(SensorDaten &daten) {
    // SHT40, SPS30, VEML immer lesen
    sht4x.measureHighPrecision(daten.tempSht, daten.humSht);
    daten.dewPoint    = calcDewPoint(daten.tempSht, daten.humSht);
    daten.absHumidity = calcAbsHumidity(daten.tempSht, daten.humSht);
    daten.lux = vemlOK ? veml.readLux(VEML_LUX_AUTO) : 100.0f;
    // SPS30 ...

    if (sps30OK) {
        uint16_t dataReadyFlag = 0;
        sps30.readDataReadyFlag(dataReadyFlag);
        if (dataReadyFlag) {
            uint16_t mc1p0 = 0, mc2p5 = 0, mc4p0 = 0, mc10p0 = 0;
            uint16_t nc0p5 = 0, nc1p0 = 0, nc2p5 = 0, nc4p0 = 0, nc10p0 = 0;
            uint16_t typicalParticleSize = 0;
            int16_t err = sps30.readMeasurementValuesUint16(mc1p0, mc2p5, mc4p0, mc10p0, nc0p5, nc1p0, nc2p5, nc4p0, nc10p0, typicalParticleSize);
            if (err == NO_ERROR) {daten.pm10  = mc1p0  / 10.0f; daten.pm25  = mc2p5  / 10.0f; daten.pm100 = mc10p0 / 10.0f;}
            }
        }

    // SCD41 nur wenn bereit
    bool dataReady = false;
    scd4x.getDataReadyStatus(dataReady);
    if (dataReady) {
        scd4x.readMeasurement(daten.co2, daten.tempScd, daten.humScd);
    }

    //Serial.println("==========Messwerte=============");
    //Serial.print("Lux: ");    Serial.println(daten.lux);
    //Serial.print("PM1.0: ");  Serial.println(daten.pm10);
    //Serial.print("PM2.5: ");  Serial.println(daten.pm25);
    //Serial.print("PM10: ");   Serial.println(daten.pm100);
    //Serial.print("CO2: ");    Serial.println(daten.co2);
    //Serial.print("Temp: ");   Serial.println(daten.tempSht);
    //Serial.print("RH: ");     Serial.println(daten.humSht);    


    return true; // immer true → Display updated immer
}

