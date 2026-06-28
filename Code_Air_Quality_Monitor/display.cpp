#include "display.h"
#include "config.h"
#include "espnow.h"
#include <TFT_eSPI.h>
#include <math.h>  // [NEU] für log(), exp()

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite kachel = TFT_eSprite(&tft); 
TFT_eSprite graph24h = TFT_eSprite(&tft);
TFT_eSprite outdoor = TFT_eSprite(&tft);
TFT_eSprite infoKachel = TFT_eSprite(&tft);  // [NEU] mittlere Info-Kachel

// === KONFIGURATION & GLOBALE VARIABLEN ===
int tft_w, tft_h;
int top_bar_h, kachel_w, kachel_h, bottom_y, bottom_h;

const int HIST_LEN   = 60;
const int HIST24_LEN = 144;

float histCO2[HIST_LEN], histRH[HIST_LEN], histTemp[HIST_LEN];
float histPM1[HIST_LEN], histPM25[HIST_LEN], histPM10[HIST_LEN];
float histTVOC[HIST_LEN];
float h24_CO2[HIST24_LEN], h24_Temp[HIST24_LEN], h24_RH[HIST24_LEN];

unsigned long letzter24hEintrag  = 0;
unsigned long letzterKurzEintrag = 0;
bool arraysInitialisiert = false;

// === FARBEN ===
uint16_t COLOR_BG        = TFT_BLACK;
uint16_t COLOR_CARD      = tft.color565(28, 30, 38);
uint16_t COLOR_TEXT_MUTED= tft.color565(150, 160, 175);
uint16_t COLOR_LINE      = tft.color565(50, 55, 65);
uint16_t C_GREEN  = tft.color565(46, 204, 113);
uint16_t C_YELLOW = tft.color565(241, 196, 15);
uint16_t C_RED    = tft.color565(231, 76, 60);
uint16_t C_CYAN   = tft.color565(52, 152, 219);

// === [NEU] BERECHNUNGSFUNKTIONEN ===

// Absolute Luftfeuchte in g/m³ (Magnus-Näherung)
float absLuftfeuchte(float tempC, float relHum) {
    return (6.112f * exp((17.67f * tempC) / (tempC + 243.5f)) * relHum * 2.1674f)
           / (273.15f + tempC);
}

// Taupunkt in °C (Magnus-Formel)
float taupunkt(float tempC, float relHum) {
    if (relHum <= 0.0f) relHum = 0.01f;
    float a = 17.67f, b = 243.5f;
    float gamma = log(relHum / 100.0f) + (a * tempC) / (b + tempC);
    return (b * gamma) / (a - gamma);
}

// Heat Index in °C (Steadman/Rothfusz, gültig ab ~27°C / RH>40%)
// Unterhalb sinnloser Bereiche: einfach Temperatur zurückgeben
float heatIndex(float tempC, float rh) {
    if (tempC < 27.0f || rh < 40.0f) return tempC;
    float T = tempC * 9.0f / 5.0f + 32.0f;
    float HI = -42.379f
             +  2.04901523f * T
             + 10.14333127f * rh
             -  0.22475541f * T * rh
             -  0.00683783f * T * T
             -  0.05481717f * rh * rh
             +  0.00122874f * T * T * rh
             +  0.00085282f * T * rh * rh
             -  0.00000199f * T * T * rh * rh;
    return (HI - 32.0f) * 5.0f / 9.0f;
}

// [NEU] Luftqualitätsindex 0–5 aus CO2 + TVOC
// 0=Sehr gut, 1=Gut, 2=Mäßig, 3=Schlecht, 4=Sehr schlecht, 5=Gefährlich
int luftqualitaetsIndex(float co2, float tvoc) {
    int co2Stufe = 0;
    if      (co2 >= 2000) co2Stufe = 5;
    else if (co2 >= 1500) co2Stufe = 4;
    else if (co2 >= 1200) co2Stufe = 3;
    else if (co2 >= 1000) co2Stufe = 2;
    else if (co2 >= 800)  co2Stufe = 1;

    int tvocStufe = 0;
    if      (tvoc >= 2200) tvocStufe = 5;
    else if (tvoc >= 1430) tvocStufe = 4;
    else if (tvoc >= 660)  tvocStufe = 3;
    else if (tvoc >= 220)  tvocStufe = 2;
    else if (tvoc >= 65)   tvocStufe = 1;

    return max(co2Stufe, tvocStufe);
}

// [NEU] Lüftungsempfehlung:
// 0=Nicht nötig (grün), 1=Empfohlen (gelb), 2=Dringend (rot)
int lueEmpfehlung(float co2, float tvoc, float innenAbs, float aussenAbs) {
    int aqi = luftqualitaetsIndex(co2, tvoc);
    // Lüften bringt was, wenn Außenluft besser (weniger abs. Feuchte oder Feuchte OK)
    bool aussenBesser = (aussenAbs < innenAbs * 0.95f) || (innenAbs > 12.0f);
    if (aqi >= 3) return 2; // dringend
    if (aqi >= 1) return aussenBesser ? 1 : 0;
    return 0;
}

// === HILFSFUNKTIONEN ===
void addValue(float* arr, int len, float val) {
    for (int i = 0; i < len - 1; i++) arr[i] = arr[i + 1];
    arr[len - 1] = val;
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
    if (x < in_min) x = in_min;
    if (x > in_max) x = in_max;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// === SYMBOLE ===
void drawWifiSymbol(int cx, int cy, bool connected) {
    uint16_t col = connected ? C_GREEN : C_RED;
    tft.fillRect(cx-10, cy-5, 20, 22, COLOR_BG);
    tft.fillCircle(cx, cy+12, 2, col);
    tft.drawArc(cx, cy+12, 6,  4,  45, 135, col, COLOR_BG);
    tft.drawArc(cx, cy+12, 11, 9,  45, 135, col, COLOR_BG);
    tft.drawArc(cx, cy+12, 16, 14, 45, 135, col, COLOR_BG);
}

void drawOutdoorSymbol(int cx, int cy, bool connected) {
    uint16_t col = connected ? C_GREEN : C_RED;
    tft.fillRect(cx-10, cy-2, 20, 22, COLOR_BG);
    tft.drawLine(cx,   cy,    cx-8, cy+8, col);
    tft.drawLine(cx,   cy,    cx+8, cy+8, col);
    tft.drawLine(cx-8, cy+8,  cx-8, cy+18, col);
    tft.drawLine(cx+8, cy+8,  cx+8, cy+18, col);
    tft.drawLine(cx-8, cy+18, cx+8, cy+18, col);
    tft.drawRect(cx-3, cy+11, 6, 7, col);
}

// === DISPLAY FUNKTIONEN ===
void initDisplay() {
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, LOW);  delay(50);
    digitalWrite(TFT_RST, HIGH); delay(150);
    tft.init();
    tft.setRotation(ORIENTATION);
    tft.fillScreen(COLOR_BG);
    tft_w = tft.width();  tft_h = tft.height();
    
    top_bar_h = 45; 
    kachel_w  = tft_w / 5;
    // [GEÄNDERT] Kacheln wieder etwas größer (52% des Platzes)
    kachel_h  = (tft_h - top_bar_h) * 0.52; 
    bottom_y  = top_bar_h + kachel_h;
    bottom_h  = tft_h - bottom_y;
}

void displayAufwachen() { tft.writecommand(0x11); delay(120); zeichneDashboardRaster(); }
void zeichneDashboardRaster() { tft.fillScreen(COLOR_BG); }

void updateTopBar(const char* datumStr, const char* zeitStr, bool wlanVerbunden, bool outdoorVerbunden) {
    tft.setTextPadding(130); tft.setTextColor(COLOR_TEXT_MUTED, COLOR_BG);
    tft.setTextDatum(ML_DATUM); tft.drawString(datumStr, 12, top_bar_h/2, 2);
    
    tft.setTextPadding(160); tft.setTextColor(TFT_WHITE, COLOR_BG);
    // [GEÄNDERT] Uhrzeit wieder groß (Font 6) und passend zentriert
    tft.setTextDatum(MC_DATUM); tft.drawString(zeitStr, tft_w/2, top_bar_h/2 + 4, 6); 
    
    drawWifiSymbol(tft_w - 45, top_bar_h/2 - 12, wlanVerbunden);
    drawOutdoorSymbol(tft_w - 18, top_bar_h/2 - 10, outdoorVerbunden);
}

// === GRAPHEN ===
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

// === KACHELN ===
void updateSensorKacheln(const SensorDaten &daten) {
    if (!arraysInitialisiert && daten.co2 > 0) {
        for (int i = 0; i < HIST_LEN; i++) {
            histCO2[i]=daten.co2; histRH[i]=daten.humSht; histTemp[i]=daten.tempSht;
            histPM1[i]=daten.pm10; histPM25[i]=daten.pm25; histPM10[i]=daten.pm100;
            histTVOC[i]=daten.tvoc;
        }
        for (int i = 0; i < HIST24_LEN; i++) {
            h24_CO2[i]=daten.co2; h24_Temp[i]=daten.tempSht; h24_RH[i]=daten.humSht;
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
        addValue(histTVOC, HIST_LEN, daten.tvoc);
    }

    for (int k = 0; k < 5; k++) {
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
        } else if (k==3) {
            titelName=""; titelEinheit=""; wert="";
            farbe=(daten.pm25>=pm25_th_critical||daten.pm100>=pm10_th_critical)?C_RED:(daten.pm25>=pm25_th_alert||daten.pm100>=pm10_th_alert)?C_YELLOW:C_GREEN;
            arr=histPM25; minV=GRAPH_PM_MIN; maxV=GRAPH_PM_MAX;
        } else {
            titelName="TVOC"; titelEinheit="[ppb]"; wert=String(daten.tvoc);
            farbe=(daten.tvoc>=tvoc_th_critical)?C_RED:(daten.tvoc>=tvoc_th_alert)?C_YELLOW:C_GREEN;
            arr=histTVOC; minV=GRAPH_TVOC_MIN; maxV=GRAPH_TVOC_MAX;
        }

        if (k != 3) {
            kachel.setTextDatum(TC_DATUM); kachel.setTextColor(COLOR_TEXT_MUTED);
            kachel.drawString(titelName,    kachel_w/2, 6,  2);
            kachel.drawString(titelEinheit, kachel_w/2, 20, 1);
            kachel.setTextDatum(MC_DATUM); kachel.setTextColor(farbe);
            kachel.drawString(wert, kachel_w/2, kachel_h/2-2, 4);
        } 
        else {
            // [GEÄNDERT] PM Labels größer (Font 2), Abstand vergrößert, 1 Nachkommastelle
            int yBase = 13;   
            int step  = 26;   // Mehr Abstand, da Font 4 sehr hoch ist
            
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

// === [NEU] MITTLERE INFO-KACHEL ===
void updateInfoKachel(const SensorDaten &daten, const OutdoorDaten &outdoor_daten) {
    int info_w   = tft_w / 3;
    int info_x   = tft_w / 3;

    float dp     = taupunkt(daten.tempSht, daten.humSht);
    float absIn  = absLuftfeuchte(daten.tempSht, daten.humSht);
    float absOut = outdoor_daten.gueltig
                     ? absLuftfeuchte(outdoor_daten.temp, outdoor_daten.hum)
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

    // [GEÄNDERT] Schriftgröße der Labels von 1 auf 2 vergrößert
    infoKachel.setTextColor(COLOR_TEXT_MUTED);
    infoKachel.drawString("Taupunkt", c1, r1, 2);
    infoKachel.drawString("Abs. LF", c2, r1, 2);
    infoKachel.setTextColor(TFT_WHITE);
    infoKachel.drawString(String(dp, 1) + "C", c1, r1 + 14, 2);
    infoKachel.setTextColor(C_CYAN);
    infoKachel.drawString(String(absIn, 1) + "g", c2, r1 + 14, 2);

    infoKachel.setTextColor(COLOR_TEXT_MUTED);
    infoKachel.drawString("Heat Idx", c1, r2, 2);
    infoKachel.drawString("Luftgute", c2, r2, 2);
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
    infoKachel.drawString("Status", c2, r3, 2);
    infoKachel.setTextColor(lueftFarbe);
    infoKachel.drawString(lueftText[lueft], c1, r3 + 14, 2);
    infoKachel.fillCircle(c2, r3 + 20, 6, lueftFarbe); 

    infoKachel.pushSprite(info_x, bottom_y);
    infoKachel.deleteSprite();
}

// === UNTERER BEREICH ===
void updateUntererBereich(const SensorDaten &daten) {
    int uW     = tft_w / 3;
    int info_w = tft_w / 3;
    int out_w  = tft_w - uW - info_w;

    if (arraysInitialisiert && millis() - letzter24hEintrag > 600000UL) {
        letzter24hEintrag = millis();
        addValue(h24_CO2,  HIST24_LEN, daten.co2);
        addValue(h24_Temp, HIST24_LEN, daten.tempSht);
        addValue(h24_RH,   HIST24_LEN, daten.humSht);
    }

    graph24h.createSprite(uW, bottom_h);
    graph24h.fillSprite(COLOR_BG);
    graph24h.fillRoundRect(3, 3, uW-6, bottom_h-6, 8, COLOR_CARD);
    
    graph24h.setTextDatum(TL_DATUM); graph24h.setTextColor(TFT_WHITE);
    graph24h.drawString("24h Verlauf", 12, 10, 2);
    
    graph24h.setTextDatum(TR_DATUM);
    graph24h.setTextColor(C_RED);    graph24h.drawString("CO2",  uW-45, 12, 1);
    graph24h.setTextColor(C_YELLOW); graph24h.drawString("T", uW-28, 12, 1);
    graph24h.setTextColor(C_CYAN);   graph24h.drawString("RH",   uW-12, 12, 1);

    int gY = 35; 
    int gH = bottom_h - 56; 

    graph24h.setTextDatum(TC_DATUM);
    for (int i = 0; i <= 4; i++) {
        int x = 15 + i * (uW - 30) / 4;
        graph24h.drawFastVLine(x, gY, gH, COLOR_LINE); 
        if (i % 2 == 0) { 
            String lbl = (i == 0) ? "-24h" : (i == 2) ? "-12h" : "0h";
            graph24h.setTextColor(COLOR_TEXT_MUTED);
            graph24h.drawString(lbl, x, gY + gH + 4, 1);
        }
    }
    for (int i = 0; i <= 2; i++) {
        int y = gY + i * gH / 2;
        graph24h.drawFastHLine(15, y, uW - 30, COLOR_LINE); 
    }

    float co2Min=h24_CO2[0],  co2Max=h24_CO2[0];
    float tMin=h24_Temp[0],   tMax=h24_Temp[0];
    float rhMin=h24_RH[0],    rhMax=h24_RH[0];
    for (int i = 1; i < HIST24_LEN; i++) {
        co2Min=min(co2Min,h24_CO2[i]);  co2Max=max(co2Max,h24_CO2[i]);
        tMin=min(tMin,h24_Temp[i]);     tMax=max(tMax,h24_Temp[i]);
        rhMin=min(rhMin,h24_RH[i]);     rhMax=max(rhMax,h24_RH[i]);
    }
    auto addPuffer = [](float &mn, float &mx, float hardMin, float hardMax) {
        float p = max((mx-mn)*0.1f, 1.0f);
        mn=max(mn-p,hardMin); mx=min(mx+p,hardMax);
    };
    addPuffer(co2Min,co2Max,(float)GRAPH_CO2_MIN, (float)GRAPH_CO2_MAX);
    addPuffer(tMin,  tMax,  (float)GRAPH_TEMP_MIN,(float)GRAPH_TEMP_MAX);
    addPuffer(rhMin, rhMax, (float)GRAPH_RH_MIN,  (float)GRAPH_RH_MAX);

    for (int i = 0; i < HIST24_LEN-1; i++) {
        int x1=map(i,  0,HIST24_LEN-1,15,uW-15);
        int x2=map(i+1,0,HIST24_LEN-1,15,uW-15);
        graph24h.drawLine(x1,gY+gH-mapFloat(h24_CO2[i],  co2Min,co2Max,0,gH),
                          x2,gY+gH-mapFloat(h24_CO2[i+1],co2Min,co2Max,0,gH),C_RED);
        graph24h.drawLine(x1,gY+gH-mapFloat(h24_Temp[i],  tMin,tMax,0,gH),
                          x2,gY+gH-mapFloat(h24_Temp[i+1],tMin,tMax,0,gH),C_YELLOW);
        graph24h.drawLine(x1,gY+gH-mapFloat(h24_RH[i],  rhMin,rhMax,0,gH),
                          x2,gY+gH-mapFloat(h24_RH[i+1],rhMin,rhMax,0,gH),C_CYAN);
    }
    graph24h.pushSprite(0, bottom_y);
    graph24h.deleteSprite();

    OutdoorDaten outdoor_daten = getOutdoorDaten();
    updateInfoKachel(daten, outdoor_daten);

    outdoor.createSprite(out_w, bottom_h);
    outdoor.fillSprite(COLOR_BG);
    outdoor.fillRoundRect(3, 3, out_w-6, bottom_h-6, 8, COLOR_CARD);
    
    outdoor.setTextDatum(TC_DATUM); outdoor.setTextColor(COLOR_TEXT_MUTED);
    outdoor.drawString("Draussen", out_w/2, 10, 2);

    if (!outdoor_daten.gueltig) {
        outdoor.setTextDatum(MC_DATUM); outdoor.setTextColor(TFT_WHITE);
        outdoor.drawString("Warten...", out_w/2, bottom_h/2, 2);
    } else {
        float absOut = absLuftfeuchte(outdoor_daten.temp, outdoor_daten.hum);

        // Zeilenabstand leicht erhöht wegen größerer Zahlen
        int yPos   = bottom_h * 0.28; 
        int step   = bottom_h * 0.18; 
        int xLabel = 10;
        int xWert  = out_w - 10;

        auto printRow = [&](const char* lbl, String val, uint16_t color) {
            outdoor.setTextDatum(ML_DATUM);
            outdoor.setTextColor(COLOR_TEXT_MUTED);
            outdoor.drawString(lbl, xLabel, yPos, 2); 

            // [GEÄNDERT] Messwerte außen auf Font 4 (sehr groß) hochgesetzt
            outdoor.setTextDatum(MR_DATUM);
            outdoor.setTextColor(color);
            outdoor.drawString(val, xWert, yPos, 2); 
            yPos += step;
        };

        // Das "hPa" weggelassen, damit der Druck in Font 4 nicht mit dem Label überlappt
        printRow("Temp",  String(outdoor_daten.temp, 1) + "C", TFT_WHITE);
        printRow("RH",    String(outdoor_daten.hum, 0) + "%", C_CYAN);
        printRow("Abs",   String(absOut, 1) + "g", C_CYAN);
        printRow("Druck", String(outdoor_daten.pressure, 0), TFT_WHITE); 
    }

    outdoor.pushSprite(tft_w - out_w, bottom_y);
    outdoor.deleteSprite();
}