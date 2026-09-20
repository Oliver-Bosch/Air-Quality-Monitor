#include "sensor.h"
#include "config.h"
#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include <SensirionI2cSht4x.h>
#include <SensirionI2cSps30.h>
#include <Adafruit_VEML7700.h>
#include <cmath>
#include "utils.h"

#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0
static bool first = true;
const float alpha = 0.3f;

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
    int16_t err = sps30.startMeasurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_FLOAT);
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
        float mc1p0 = 0, mc2p5 = 0, mc4p0 = 0, mc10p0 = 0;
        float nc0p5 = 0, nc1p0 = 0, nc2p5 = 0, nc4p0 = 0, nc10p0 = 0;
        float typicalParticleSize = 0;
        int16_t err = sps30.readMeasurementValuesFloat(mc1p0, mc2p5, mc4p0, mc10p0,
                                                       nc0p5, nc1p0, nc2p5, nc4p0,
                                                       nc10p0, typicalParticleSize);
        if (err == NO_ERROR) {
            if (first) {
                daten.pm10  = mc1p0;
                daten.pm25  = mc2p5;
                daten.pm100 = mc10p0;
                first = false;
            } else {
                daten.pm10  = alpha * mc1p0  + (1.0f - alpha) * daten.pm10;
                daten.pm25  = alpha * mc2p5  + (1.0f - alpha) * daten.pm25;
                daten.pm100 = alpha * mc10p0 + (1.0f - alpha) * daten.pm100;
            }
        }
    }
}

    // SCD41 nur wenn bereit
    bool dataReady = false;
    scd4x.getDataReadyStatus(dataReady);
    if (dataReady) {
        scd4x.readMeasurement(daten.co2, daten.tempScd, daten.humScd);
    }

    Serial.println("==========Messwerte=============");
    Serial.print("Lux: ");    Serial.println(daten.lux);
    Serial.print("PM1.0: ");  Serial.println(daten.pm10);
    Serial.print("PM2.5: ");  Serial.println(daten.pm25);
    Serial.print("PM10: ");   Serial.println(daten.pm100);
    Serial.print("CO2: ");    Serial.println(daten.co2);
    Serial.print("Temp: ");   Serial.print(daten.tempSht); Serial.print(" | "); Serial.println(daten.tempScd);
    Serial.print("RH: ");     Serial.print(daten.humSht); Serial.print(" | "); Serial.println(daten.humScd);


    return true; // immer true → Display updated immer
}

