#include "display.h"
#include "config.h"
#include "espnow.h"
#include "weather.h"
#include <TFT_eSPI.h>
#include <math.h>  
#include "utils.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite kachel = TFT_eSprite(&tft); 
TFT_eSprite graph24h = TFT_eSprite(&tft);
TFT_eSprite outdoor = TFT_eSprite(&tft);
TFT_eSprite infoKachel = TFT_eSprite(&tft);  

// === KONFIGURATION & GLOBALE VARIABLEN ===
int tft_w, tft_h;
int top_bar_h, kachel_w, kachel_h, bottom_y, bottom_h;

const int HIST_LEN   = 60;

float histCO2[HIST_LEN], histRH[HIST_LEN], histTemp[HIST_LEN];
float histPM1[HIST_LEN], histPM25[HIST_LEN], histPM10[HIST_LEN];

unsigned long letzterKurzEintrag = 0;
bool arraysInitialisiert = false;

const int PRESSURE_HIST_LEN = 180;   // 3 Hours

float histPressureAMSL[PRESSURE_HIST_LEN];
bool pressureInitialisiert = false;

float pressureTrend = 0.0f;
String weatherTrend = "Stabil";


// === FARBEN ===
uint16_t COLOR_BG        = TFT_BLACK;
uint16_t COLOR_CARD      = tft.color565(28, 30, 38);
uint16_t COLOR_TEXT_MUTED= tft.color565(150, 160, 175);
uint16_t COLOR_LINE      = tft.color565(50, 55, 65);
uint16_t C_GREEN  = tft.color565(46, 204, 113);
uint16_t C_YELLOW = tft.color565(241, 196, 15);
uint16_t C_RED    = tft.color565(231, 76, 60);
uint16_t C_CYAN   = tft.color565(52, 152, 219);

// === DISPLAY FUNKTIONEN ===

void drawMiniGraph(TFT_eSprite &spr, float* data, uint16_t color, float minV, float maxV) {
    float dynMin = data[0], dynMax = data[0];
    for (int i = 1; i < HIST_LEN; i++) {
        if (data[i] < dynMin) dynMin = data[i];
        if (data[i] > dynMax) dynMax = data[i];
    }
    float puffer = (dynMax - dynMin) * 0.2f;
    if (puffer < 1.0f) puffer = 1.0f;
    dynMin -= puffer; dynMax += puffer;
    if (dynMin < minV) dynMin = minV;
    if (dynMax > maxV) dynMax = maxV;

    int graphH = kachel_h * 0.28;
    int startY = kachel_h - graphH - 8;
    spr.drawFastHLine(6, kachel_h - 6, kachel_w - 12, COLOR_LINE);
    for (int i = 0; i < HIST_LEN - 1; i++) {
        int x1 = map(i,   0, HIST_LEN-1, 6, kachel_w-6);
        int x2 = map(i+1, 0, HIST_LEN-1, 6, kachel_w-6);
        int y1 = startY + graphH - mapFloat(data[i],   dynMin, dynMax, 0, graphH);
        int y2 = startY + graphH - mapFloat(data[i+1], dynMin, dynMax, 0, graphH);
        spr.drawLine(x1, y1,   x2, y2,   color);
        spr.drawLine(x1, y1+1, x2, y2+1, color);
    }
}

void drawMiniGraphPM(TFT_eSprite &spr, float minV, float maxV) {
    float dynMin = histPM1[0], dynMax = histPM1[0];
    for (int i = 0; i < HIST_LEN; i++) {
        dynMin = min(dynMin, min(histPM1[i], min(histPM25[i], histPM10[i])));
        dynMax = max(dynMax, max(histPM1[i], max(histPM25[i], histPM10[i])));
    }
    float puffer = max((dynMax - dynMin) * 0.2f, 1.0f);
    dynMin = max(dynMin - puffer, minV);
    dynMax = min(dynMax + puffer, maxV);

    int graphH = kachel_h * 0.28;
    int startY = kachel_h - graphH - 8;
    spr.drawFastHLine(6, kachel_h - 6, kachel_w - 12, COLOR_LINE);
    for (int i = 0; i < HIST_LEN - 1; i++) {
        int x1 = map(i,   0, HIST_LEN-1, 6, kachel_w-6);
        int x2 = map(i+1, 0, HIST_LEN-1, 6, kachel_w-6);
        spr.drawLine(x1, startY+graphH-mapFloat(histPM1[i],   dynMin,dynMax,0,graphH),
                     x2, startY+graphH-mapFloat(histPM1[i+1], dynMin,dynMax,0,graphH), C_CYAN);
        spr.drawLine(x1, startY+graphH-mapFloat(histPM25[i],  dynMin,dynMax,0,graphH),
                     x2, startY+graphH-mapFloat(histPM25[i+1],dynMin,dynMax,0,graphH), C_YELLOW);
        spr.drawLine(x1, startY+graphH-mapFloat(histPM10[i],  dynMin,dynMax,0,graphH),
                     x2, startY+graphH-mapFloat(histPM10[i+1],dynMin,dynMax,0,graphH), C_RED);
    }
}

// ==========================================
// Symbole
// ==========================================

void drawWifiSymbol(int cx, int cy, bool connected) {
    uint16_t col = connected ? C_GREEN : C_RED;
    tft.fillRect(cx-10, cy-5, 20, 22, COLOR_BG);
    tft.fillCircle(cx, cy+12, 2, col);
    tft.drawArc(cx, cy+12, 6,  4,  45, 135, col, COLOR_BG);
    tft.drawArc(cx, cy+12, 11, 9,  45, 135, col, COLOR_BG);
    tft.drawArc(cx, cy+12, 16, 14, 45, 135, col, COLOR_BG);
}




// ==========================================
// Weather
// ==========================================


void updatePressureTrend(float pressureAMSL){
    if (!pressureInitialisiert)
    {
        for (int i = 0; i < PRESSURE_HIST_LEN; i++)
            histPressureAMSL[i] = pressureAMSL;

        pressureInitialisiert = true;
        return;
    }

    addValue(histPressureAMSL, PRESSURE_HIST_LEN, pressureAMSL);

    pressureTrend = histPressureAMSL[PRESSURE_HIST_LEN - 1] - histPressureAMSL[0];

    if (pressureAMSL < 995 && pressureTrend < -1.0f)
        weatherTrend = "Sturm";
    else if (pressureAMSL < 1005 && pressureTrend < -2.0f)
        weatherTrend = "Regen";
    else if (pressureAMSL > 1025 && pressureTrend > 0.0f)
        weatherTrend = "Sonnig";
    else if (pressureTrend >= 3.0f)
        weatherTrend = "Besser";
    else if (pressureTrend >= 1.0f)
        weatherTrend = "Leicht +";
    else if (pressureTrend <= -3.0f)
        weatherTrend = "Schlechter";
    else if (pressureTrend <= -1.0f)
        weatherTrend = "Leicht -";
    else
        weatherTrend = "Stabil";
}


// ==========================================
// Tiles
// ==========================================

void initDisplay() {
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, LOW);  delay(50);
    digitalWrite(TFT_RST, HIGH); delay(150);
    tft.init();
    tft.setRotation(ORIENTATION);
    tft.fillScreen(COLOR_BG);
    tft_w = tft.width();  tft_h = tft.height();
    
    top_bar_h = 45; 
    kachel_w  = tft_w / 4;
    kachel_h  = (tft_h - top_bar_h) * 0.52; 
    bottom_y  = top_bar_h + kachel_h;
    bottom_h  = tft_h - bottom_y;
}

void displayAufwachen() { tft.writecommand(0x11); delay(120); zeichneDashboardRaster(); }
void zeichneDashboardRaster() { tft.fillScreen(COLOR_BG); }

void updateTopBar(const char* datumStr, const char* zeitStr, bool wlanVerbunden) {
    tft.setTextPadding(130); tft.setTextColor(COLOR_TEXT_MUTED, COLOR_BG);
    tft.setTextDatum(ML_DATUM); tft.drawString(datumStr, 12, top_bar_h/2, 2);
    
    tft.setTextPadding(0);
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    tft.setTextDatum(MC_DATUM); tft.drawString(zeitStr, tft_w/2, top_bar_h/2 + 2, 6); 
    
    static bool letzterWlan    = -1;
    static bool letzterOutdoor = -1;

    if (wlanVerbunden != letzterWlan) {
        drawWifiSymbol(tft_w - 45, top_bar_h/2 - 12, wlanVerbunden);
        letzterWlan = wlanVerbunden;
    }
}


void updateSensorKacheln(const SensorDaten &daten) {
    if (!arraysInitialisiert && daten.co2 > 0) {
        for (int i = 0; i < HIST_LEN; i++) {
            histCO2[i]=daten.co2; histRH[i]=daten.humSht; histTemp[i]=daten.tempSht;
            histPM1[i]=daten.pm10; histPM25[i]=daten.pm25; histPM10[i]=daten.pm100;
        }
        arraysInitialisiert = true;
    }

    if (arraysInitialisiert && millis() - letzterKurzEintrag > 60000UL) {
        letzterKurzEintrag = millis();
        addValue(histCO2,  HIST_LEN, daten.co2);
        addValue(histRH,   HIST_LEN, daten.humSht);
        addValue(histTemp, HIST_LEN, daten.tempSht);
        addValue(histPM1,  HIST_LEN, daten.pm10);
        addValue(histPM25, HIST_LEN, daten.pm25);
        addValue(histPM10, HIST_LEN, daten.pm100);
    }

    for (int k = 0; k < 4; k++) {
        kachel.createSprite(kachel_w, kachel_h);
        kachel.fillSprite(COLOR_BG);
        kachel.fillRoundRect(3, 3, kachel_w-6, kachel_h-6, 8, COLOR_CARD);

        String titelName, titelEinheit, wert;
        uint16_t farbe; float* arr; float minV, maxV;

        if (k==0) {
            titelName="CO2"; titelEinheit="[ppm]"; wert=String(daten.co2);
            farbe=(daten.co2>=CO2_th_critical)?C_RED:(daten.co2>=CO2_th_alert)?C_YELLOW:C_GREEN;
            arr=histCO2; minV=GRAPH_CO2_MIN; maxV=GRAPH_CO2_MAX;
        } else if (k==1) {
            titelName="RH"; titelEinheit="[%]"; wert=String(daten.humSht,1)+"%";
            farbe=(daten.humSht>=RH_th_critical)?C_RED:(daten.humSht<=RH_th_alert_low)?C_YELLOW:C_GREEN;
            arr=histRH; minV=GRAPH_RH_MIN; maxV=GRAPH_RH_MAX;
        } else if (k==2) {
            titelName="TEMP"; titelEinheit="[C]"; wert=String(daten.tempSht,1)+"°";
            farbe=(daten.tempSht>=TEMP_th_critical)?C_RED:(daten.tempSht>=TEMP_th_alert_high||daten.tempSht<=TEMP_th_alert_low)?C_YELLOW:C_GREEN;
            arr=histTemp; minV=GRAPH_TEMP_MIN; maxV=GRAPH_TEMP_MAX;
        } else {
            titelName=""; titelEinheit=""; wert="";
            farbe=(daten.pm25>=pm25_th_critical||daten.pm100>=pm10_th_critical)?C_RED:(daten.pm25>=pm25_th_alert||daten.pm100>=pm10_th_alert)?C_YELLOW:C_GREEN;
            arr=histPM25; minV=GRAPH_PM_MIN; maxV=GRAPH_PM_MAX;
        }

        if (k != 3) {
            kachel.setTextDatum(TC_DATUM); kachel.setTextColor(COLOR_TEXT_MUTED);
            kachel.drawString(titelName,    kachel_w/2, 6,  2);
            kachel.drawString(titelEinheit, kachel_w/2, 20, 1);
            kachel.setTextDatum(MC_DATUM); kachel.setTextColor(farbe);
            kachel.drawString(wert, kachel_w/2, kachel_h/2-2, 4);
        } 
        else {
            int yBase = 13;   
            int step  = 26;   
            
            kachel.setTextDatum(ML_DATUM);
            kachel.setTextColor(COLOR_TEXT_MUTED);
            kachel.drawString("PM1",   5, yBase, 2);
            kachel.drawString("PM2.5", 5, yBase + step, 2);
            kachel.drawString("PM10",  5, yBase + step*2, 2);

            kachel.setTextDatum(MR_DATUM);
            kachel.setTextColor(farbe);
            kachel.drawString(String(daten.pm10, 1),  kachel_w - 5, yBase, 4);
            kachel.drawString(String(daten.pm25, 1),  kachel_w - 5, yBase + step, 4);
            kachel.drawString(String(daten.pm100, 1), kachel_w - 5, yBase + step*2, 4);
        }

        if (k==3) drawMiniGraphPM(kachel, minV, maxV);
        else      drawMiniGraph(kachel, arr, farbe, minV, maxV);

        kachel.pushSprite(k*kachel_w, top_bar_h);
        kachel.deleteSprite();
    }
}

void updateInfoKachel(const SensorDaten &daten, const OutdoorDaten &outdoor_daten) {
    int info_w   = tft_w / 3;
    int info_x   = (tft_w * 2) / 3;

    float dp     = calcDewPoint(daten.tempSht, daten.humSht);
    float absIn  = calcAbsHumidity(daten.tempSht, daten.humSht);
    float absOut = outdoor_daten.gueltig
                     ? calcAbsHumidity(outdoor_daten.temp, outdoor_daten.hum)
                     : -1.0f;
    float hi     = heatIndex(daten.tempSht, daten.humSht);
    int   aqi    = luftqualitaetsIndex(daten.co2, daten.tvoc);

    int lueft = (absOut >= 0.0f)
                  ? lueEmpfehlung(daten.co2, daten.tvoc, absIn, absOut)
                  : (aqi >= 3 ? 2 : aqi >= 1 ? 1 : 0);
    uint16_t lueftFarbe = (lueft == 2) ? C_RED : (lueft == 1) ? C_YELLOW : C_GREEN;

    uint16_t aqiFarbe = (aqi >= 3) ? C_RED : (aqi >= 1) ? C_YELLOW : C_GREEN;
    const char* aqiText[] = {"Sehr gut", "Gut", "Massig", "Schlecht", "Schlecht", "Gefahr"};
    const char* lueftText[] = {"Nein", "Empf.", "Dring!"};

    infoKachel.createSprite(info_w, bottom_h);
    infoKachel.fillSprite(COLOR_BG);
    infoKachel.fillRoundRect(3, 3, info_w-6, bottom_h-6, 8, COLOR_CARD);

    infoKachel.setTextDatum(TC_DATUM);
    infoKachel.setTextColor(COLOR_TEXT_MUTED);
    infoKachel.drawString("Klima Innen", info_w/2, 10, 2);

    int c1 = info_w / 4 + 2;        
    int c2 = info_w * 3 / 4 - 2;    
    
    int r1 = bottom_h * 0.27 - 8; 
    int r2 = bottom_h * 0.52 - 8;
    int r3 = bottom_h * 0.77 - 8;

    infoKachel.setTextDatum(TC_DATUM);

    infoKachel.setTextColor(COLOR_TEXT_MUTED);
    infoKachel.drawString("Taupunkt", c1, r1, 2);
    infoKachel.drawString("Abs. LF", c2, r1, 2);
    infoKachel.setTextColor(TFT_WHITE);
    infoKachel.drawString(String(dp, 1) + "C", c1, r1 + 14, 2);
    infoKachel.setTextColor(C_CYAN);
    infoKachel.drawString(String(absIn, 1) + "g", c2, r1 + 14, 2);

    infoKachel.setTextColor(COLOR_TEXT_MUTED);
    infoKachel.drawString("Heat Idx", c1, r2, 2);
    infoKachel.drawString("Luftqualität", c2, r2, 2);
    if (daten.tempSht >= 27.0f && daten.humSht >= 40.0f) {
        uint16_t hiFarbe = (hi >= 39.0f) ? C_RED : (hi >= 32.0f) ? C_YELLOW : C_GREEN;
        infoKachel.setTextColor(hiFarbe);
        infoKachel.drawString(String(hi, 1) + "C", c1, r2 + 14, 2);
    } else {
        infoKachel.setTextColor(COLOR_TEXT_MUTED);
        infoKachel.drawString("-", c1, r2 + 14, 2);
    }
    infoKachel.setTextColor(aqiFarbe);
    infoKachel.drawString(aqiText[aqi], c2, r2 + 14, 2);

    infoKachel.setTextColor(COLOR_TEXT_MUTED);
    infoKachel.drawString("Luften?", c1, r3, 2);

    infoKachel.setTextColor(lueftFarbe);
    infoKachel.drawString(lueftText[lueft], c1, r3 + 14, 2);

    if (outdoorVerbunden())
    {
        infoKachel.setTextColor(COLOR_TEXT_MUTED);
        infoKachel.drawString("Vorhersage", c2, r3, 2);

        uint16_t wetterFarbe = C_GREEN;

        if (weatherTrend == "Regen" || weatherTrend == "Sturm")
            wetterFarbe = C_RED;
        else if (weatherTrend == "Schlechter" || weatherTrend == "Leicht -")
            wetterFarbe = C_YELLOW;

        infoKachel.setTextColor(wetterFarbe);
        infoKachel.drawString(weatherTrend, c2, r3 + 14, 2);
    } else {
    infoKachel.fillRect(c2, r3, 10, 28, COLOR_CARD);
    }

    infoKachel.pushSprite(info_x, bottom_y);
    infoKachel.deleteSprite();
}

// === UNTERER BEREICH ===
void updateUntererBereich(const SensorDaten &daten) {
    int wetter_w = (tft_w * 2) / 3;
    int info_w   = tft_w - wetter_w;

    updateWetter();  

    // =========================================================
    // VERBUNDENE WETTER-KACHEL: JETZT + VORHERSAGE
    // =========================================================
    graph24h.createSprite(wetter_w, bottom_h);
    graph24h.fillSprite(COLOR_BG);
    graph24h.fillRoundRect(3, 3, wetter_w - 6, bottom_h - 6, 8, COLOR_CARD);

    int aktuell_w = wetter_w / 3;
    int prognose_x = aktuell_w;
    int prognose_w = wetter_w - aktuell_w;


    if (!wetterAktuell.gueltig) {
        graph24h.setTextDatum(MC_DATUM);
        graph24h.setTextColor(TFT_WHITE);
        graph24h.drawString("Warten...", aktuell_w / 2, bottom_h / 2, 2);
    } else {
        uint16_t aktFarbe = wetterFarbeFuerCode(
            wetterAktuell.weathercode, C_GREEN, C_YELLOW, C_RED, C_CYAN
        );

        graph24h.setTextDatum(MC_DATUM);
        
        // 1. Temperatur JETZT (Einheitlich Weiß)
        graph24h.setTextColor(TFT_WHITE);
        graph24h.drawString(String(wetterAktuell.temp, 1) + "C", aktuell_w / 2, 40, 4);

        // 2. Regen JETZT
        graph24h.setTextColor(C_CYAN);
        graph24h.drawString(String(wetterAktuell.regen, 1) + " mm", aktuell_w / 2, 60, 2);

        // 3. Wettertext (Jetzt in der passenden Wetter-Farbe)
        graph24h.setTextColor(aktFarbe);
        graph24h.drawString(wetterSymbolText(wetterAktuell.weathercode), aktuell_w / 2, 80, 2);

        // 4. Min/Max
        graph24h.setTextDatum(TC_DATUM);
        graph24h.setTextColor(C_CYAN);
        graph24h.drawString("H: " + String(wetterTagMax, 0), aktuell_w / 4, 102, 1);
        graph24h.setTextColor(C_YELLOW);
        graph24h.drawString("T: " + String(wetterTagMin, 0), aktuell_w * 3 / 4, 102, 1);
    }

    // --- Bereich VORHERSAGE ---
    graph24h.setTextDatum(TC_DATUM);
    graph24h.setTextColor(COLOR_TEXT_MUTED);
    graph24h.drawString(
        "Vorhersage",
        prognose_x + prognose_w / 2,
        10,
        2
    );

    if (wetterAktuell.gueltig)
    {
        // =====================================================
        // WETTERAUSWAHL
        // =====================================================

        int wetterIndex[4];
        int spalten = 0;


        // =====================================================
        // GIBT ES IN DEN NÄCHSTEN 12 STUNDEN REGEN?
        // =====================================================

        bool regenVorhanden = false;

        for (int i = 0;
            i < WEATHER_HOURLY_LEN;
            i++)
        {
            if (wetterStunden[i].regen > 0.0f)
            {
                regenVorhanden = true;
                break;
            }
        }


        // =====================================================
        // KEIN REGEN
        // =====================================================

        if (!regenVorhanden)
        {
            // Immer 4 übersichtliche Punkte
            wetterIndex[0] = 0;
            wetterIndex[1] = 3;
            wetterIndex[2] = 6;
            wetterIndex[3] = 9;

            spalten = 4;
        }


        // =====================================================
        // REGEN VORHANDEN
        // =====================================================

        else
        {
            // Nächste Stunde IMMER anzeigen
            wetterIndex[spalten++] = 0;


            // -------------------------------------------------
            // Die 3 stärksten Regenstunden suchen
            // -------------------------------------------------

            int beste[3] = {-1, -1, -1};

            float besterRegen[3] = {
                -1.0f,
                -1.0f,
                -1.0f
            };


            for (int i = 1;
                i < WEATHER_HOURLY_LEN;
                i++)
            {
                float regen =
                    wetterStunden[i].regen;

                if (regen <= 0.0f)
                    continue;


                for (int j = 0; j < 3; j++)
                {
                    if (regen > besterRegen[j])
                    {
                        for (int k = 2; k > j; k--)
                        {
                            besterRegen[k] =
                                besterRegen[k - 1];

                            beste[k] =
                                beste[k - 1];
                        }

                        besterRegen[j] = regen;
                        beste[j] = i;

                        break;
                    }
                }
            }


            // -------------------------------------------------
            // Regenstunden chronologisch hinzufügen
            // -------------------------------------------------

            for (int i = 1;
                i < WEATHER_HOURLY_LEN;
                i++)
            {
                for (int j = 0; j < 3; j++)
                {
                    if (beste[j] == i)
                    {
                        wetterIndex[spalten++] = i;
                        break;
                    }
                }

                if (spalten >= 4)
                    break;
            }


            // -------------------------------------------------
            // Falls weniger als 4 Anzeigen vorhanden sind:
            // mit trockenen Stunden auffüllen
            // -------------------------------------------------

            int auffuellStunden[] = {
                3, 6, 9
            };


            for (int a = 0;
                a < 3 && spalten < 4;
                a++)
            {
                int kandidat =
                    auffuellStunden[a];


                if (kandidat >= WEATHER_HOURLY_LEN)
                    continue;


                bool vorhanden = false;

                for (int j = 0;
                    j < spalten;
                    j++)
                {
                    if (wetterIndex[j] == kandidat)
                    {
                        vorhanden = true;
                        break;
                    }
                }


                if (!vorhanden)
                {
                    wetterIndex[spalten++] =
                        kandidat;
                }
            }


            // -------------------------------------------------
            // Chronologisch sortieren
            // -------------------------------------------------

            for (int i = 0; i < spalten - 1; i++)
            {
                for (int j = i + 1;
                    j < spalten;
                    j++)
                {
                    if (wetterIndex[j] <
                        wetterIndex[i])
                    {
                        int temp =
                            wetterIndex[i];

                        wetterIndex[i] =
                            wetterIndex[j];

                        wetterIndex[j] =
                            temp;
                    }
                }
            }
        }


        // =====================================================
        // SPALTENBREITE
        // =====================================================

        int spaltenBreite =
            (prognose_w - 20) / spalten;

        int yStunde = 32;
        int yTemp   = 52;
        int yRegen  = 72;
        int yWetter = 92;


        // =====================================================
        // VORHERSAGE ZEICHNEN
        // =====================================================

        graph24h.setTextDatum(TC_DATUM);

        for (int i = 0;
            i < spalten;
            i++)
        {
            // WICHTIG:
            // Hier NICHT mehr i * 3 verwenden!
            int index = wetterIndex[i];


            int x =
                prognose_x +
                10 +
                spaltenBreite * i +
                spaltenBreite / 2;


            uint16_t farbe =
                wetterFarbeFuerCode(
                    wetterStunden[index].weathercode,
                    C_GREEN,
                    C_YELLOW,
                    C_RED,
                    C_CYAN
                );


            // Uhrzeit
            String stundeStr =
                String(
                    wetterStunden[index].stunde
                );

            if (wetterStunden[index].stunde < 10)
                stundeStr = "0" + stundeStr;

            graph24h.setTextColor(
                COLOR_TEXT_MUTED
            );

            graph24h.drawString(
                stundeStr + ":00",
                x,
                yStunde,
                1
            );


            // Temperatur
            graph24h.setTextColor(
                TFT_WHITE
            );

            graph24h.drawString(
                String(
                    wetterStunden[index].temp,
                    0
                ) + "°",
                x,
                yTemp,
                2
            );


            // Regen
            graph24h.setTextColor(
                C_CYAN
            );

            graph24h.drawString(
                String(
                    wetterStunden[index].regen,
                    1
                ) + "mm",
                x,
                yRegen,
                1
            );


            // Wetter
            graph24h.setTextColor(
                farbe
            );

            graph24h.drawString(
                wetterSymbolText(
                    wetterStunden[index].weathercode
                ),
                x,
                yWetter,
                1
            );
        }
    }
    else
    {
        graph24h.setTextDatum(MC_DATUM);
        graph24h.setTextColor(COLOR_TEXT_MUTED);

        graph24h.drawString(
            "Keine Daten",
            prognose_x + prognose_w / 2,
            bottom_h / 2,
            2
        );
    }

    graph24h.pushSprite(0, bottom_y);
    graph24h.deleteSprite();

    // =========================================================
    // INNENKLIMA / INFO 
    // =========================================================
    int info_x = wetter_w;

    float dp     = calcDewPoint(daten.tempSht, daten.humSht);
    float absIn  = calcAbsHumidity(daten.tempSht, daten.humSht);
    float hi     = heatIndex(daten.tempSht, daten.humSht);
    int   aqi    = luftqualitaetsIndex(daten.co2, daten.tvoc);

    int lueft = (aqi >= 3 ? 2 : aqi >= 1 ? 1 : 0);
    uint16_t lueftFarbe = (lueft == 2) ? C_RED : (lueft == 1) ? C_YELLOW : C_GREEN;
    uint16_t aqiFarbe = (aqi >= 3) ? C_RED : (aqi >= 1) ? C_YELLOW : C_GREEN;
    
    const char* aqiText[] = {"Sehr gut", "Gut", "Massig", "Schlecht", "Schlecht", "Gefahr"};
    const char* lueftText[] = {"Nein", "Empf.", "Dring!"};

    infoKachel.createSprite(info_w, bottom_h);
    infoKachel.fillSprite(COLOR_BG);
    infoKachel.fillRoundRect(3, 3, info_w - 6, bottom_h - 6, 8, COLOR_CARD);

    infoKachel.setTextDatum(TC_DATUM);
    infoKachel.setTextColor(COLOR_TEXT_MUTED);
    infoKachel.drawString("Klima Innen", info_w / 2, 10, 2);

    int c1 = info_w / 4 + 2;
    int c2 = info_w * 3 / 4 - 2;

    int r1 = bottom_h * 0.27 - 8;
    int r2 = bottom_h * 0.52 - 8;
    int r3 = bottom_h * 0.77 - 8;

    infoKachel.setTextDatum(TC_DATUM);
    infoKachel.setTextColor(COLOR_TEXT_MUTED);
    infoKachel.drawString("Taupunkt", c1, r1, 2);
    infoKachel.drawString("Abs. LF", c2, r1, 2);
    infoKachel.setTextColor(TFT_WHITE);
    infoKachel.drawString(String(dp, 1) + "C", c1, r1 + 14, 2);
    infoKachel.setTextColor(C_CYAN);
    infoKachel.drawString(String(absIn, 1) + "g", c2, r1 + 14, 2);

    infoKachel.setTextColor(COLOR_TEXT_MUTED);
    infoKachel.drawString("Heat Idx", c1, r2, 2);
    infoKachel.drawString("Luftqualität", c2, r2, 2);

    if (daten.tempSht >= 27.0f && daten.humSht >= 40.0f) {
        uint16_t hiFarbe = (hi >= 39.0f) ? C_RED : (hi >= 32.0f) ? C_YELLOW : C_GREEN;
        infoKachel.setTextColor(hiFarbe);
        infoKachel.drawString(String(hi, 1) + "C", c1, r2 + 14, 2);
    } else {
        infoKachel.setTextColor(COLOR_TEXT_MUTED);
        infoKachel.drawString("-", c1, r2 + 14, 2);
    }

    infoKachel.setTextColor(aqiFarbe);
    infoKachel.drawString(aqiText[min(aqi, 5)], c2, r2 + 14, 2);

    infoKachel.setTextColor(COLOR_TEXT_MUTED);
    infoKachel.drawString("Luften?", c1, r3, 2);
    infoKachel.setTextColor(lueftFarbe);
    infoKachel.drawString(lueftText[min(lueft, 2)], c1, r3 + 14, 2);

    infoKachel.pushSprite(info_x, bottom_y);
    infoKachel.deleteSprite();
}