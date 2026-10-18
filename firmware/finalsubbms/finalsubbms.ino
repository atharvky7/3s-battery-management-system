// Simple 3S BMS with:
// - Cell voltages (A0,A1,A2)
// - Temperature (A3)
// - INA219 current sensing (optional)
// - CHARGING / DISCHARGING / IDLE detection (same as original big code)
// - Simple OV/UV/TEMP cutoff
// - Simple balancing

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <math.h>

// ---------- CONFIG ----------
const float ADC_REF = 5.0f;
const float ADC_MAX = 1023.0f;
const float DIV_RATIO = 100.0f / (330.0f + 100.0f);
const int PIN_ADC[3] = {A0, A1, A2};
const int PIN_NTC = A3;
const float V_TOP_NTC = 3.3f;

const int PIN_RELAY = 7;
const bool RELAY_ACTIVE_LOW = true;
const int PIN_BLEED[3] = {4,5,6};

const float CELL_OV_CUTOFF = 4.22f;
const float CELL_UV_CUTOFF = 2.90f;

const float BALANCE_THRESHOLD = 0.10f;  // 100mV

// Single temp window
const float TEMP_SAFE_MIN = 0.0f;
const float TEMP_SAFE_MAX = 45.0f;

// NTC params
const float R_SERIES = 10000.0f;
const float BETA = 3950.0f;
const float R_NOMINAL = 10000.0f;
const float T_NOMINAL_K = 25.0f + 273.15f;

// INA219 current sensor
Adafruit_INA219 ina219;
bool ina_present = false;

// SAME logic as your big code:
const int INA_CHARGE_SIGN = -1;
const float CHARGE_THRESH_MA = 50.0f;

// Voltage-delta fallback
float lastPackV = 0.0f;
unsigned long lastVTime = 0;
const unsigned long VDELTA_MS = 1000;
const float VDELTA_THRESH = 0.005f;

// ---------- HELPERS (defined before use) ----------
void driveRelay(bool on) {
    if (RELAY_ACTIVE_LOW) digitalWrite(PIN_RELAY, on ? LOW : HIGH);
    else digitalWrite(PIN_RELAY, on ? HIGH : LOW);
}

void setBleed(int idx, bool on) {
    if (idx < 0 || idx > 2) return;
    digitalWrite(PIN_BLEED[idx], on ? HIGH : LOW);
}

float adcToVoltage(int a) { return (a * ADC_REF) / ADC_MAX; }
float nodeToBus(float v) { return v / DIV_RATIO; }

float readCells(float &c1, float &c2, float &c3) {
    float v0 = adcToVoltage(analogRead(PIN_ADC[0]));
    float v1 = adcToVoltage(analogRead(PIN_ADC[1]));
    float v2 = adcToVoltage(analogRead(PIN_ADC[2]));

    float vb0 = nodeToBus(v0);
    float vb1 = nodeToBus(v1);
    float vb2 = nodeToBus(v2);

    c1 = vb0;
    c2 = vb1 - vb0;
    c3 = vb2 - vb1;
    return vb2;
}

float readNTC_C() {
    float v = adcToVoltage(analogRead(PIN_NTC));
    if (v < 0.0001f) return -100.0f;
    float rntc = R_SERIES * (v / (V_TOP_NTC - v));

    if (rntc <= 0.0f) return -100.0f;
    float tK = 1.0f / ((1.0f / T_NOMINAL_K) + (1.0f / BETA) * log(rntc / R_NOMINAL));
    return tK - 273.15f;
}

// ---------- CHARGE / DISCHARGE STATE ----------
String chargeStateINA(float current_mA) {
    float thr = CHARGE_THRESH_MA;
    if (INA_CHARGE_SIGN < 0) {
        if (current_mA < -thr) return "CHARGING";
        if (current_mA >  thr) return "DISCHARGING";
    } else {
        if (current_mA > thr) return "CHARGING";
        if (current_mA < -thr) return "DISCHARGING";
    }
    return "IDLE";
}

String chargeStateDelta(float packV) {
    unsigned long now = millis();
    if (now - lastVTime >= VDELTA_MS) {
        float dv = packV - lastPackV;
        lastPackV = packV;
        lastVTime = now;

        if (dv >  VDELTA_THRESH) return "CHARGING";
        if (dv < -VDELTA_THRESH) return "DISCHARGING";
        return "IDLE";
    }
    return "IDLE";
}

// ---------- SETUP ----------
void setup() {
    Serial.begin(115200);
    delay(200);

    pinMode(PIN_RELAY, OUTPUT);
    driveRelay(true);

    for (int i=0;i<3;i++) {
        pinMode(PIN_BLEED[i], OUTPUT);
        digitalWrite(PIN_BLEED[i], LOW);
    }

    // INA219 detection
    if (ina219.begin()) {
        ina_present = true;
        ina219.setCalibration_32V_2A();
        Serial.println("INA219 FOUND");
    } else {
        ina_present = false;
        Serial.println("INA219 NOT FOUND (using voltage-delta)");
    }

    float c1,c2,c3;
    lastPackV = readCells(c1,c2,c3);
    lastVTime = millis();
}

// ---------- LOOP ----------
void loop() {
    float c1,c2,c3;
    float packV = readCells(c1,c2,c3);
    float tempC = readNTC_C();

    bool ov = (c1 >= CELL_OV_CUTOFF) || (c2 >= CELL_OV_CUTOFF) || (c3 >= CELL_OV_CUTOFF);
    bool uv = (c1 <= CELL_UV_CUTOFF) || (c2 <= CELL_UV_CUTOFF) || (c3 <= CELL_UV_CUTOFF);
    bool temp_bad = (tempC < TEMP_SAFE_MIN) || (tempC > TEMP_SAFE_MAX);

    // Relay logic
    if (ov || uv || temp_bad) driveRelay(false);
    else driveRelay(true);

    bool relayOn = (digitalRead(PIN_RELAY) == (RELAY_ACTIVE_LOW ? LOW : HIGH));

    // Read current
    float current_mA = 0.0f;
    String chargeState = "IDLE";

    if (ina_present) {
        current_mA = ina219.getCurrent_mA();
        chargeState = chargeStateINA(current_mA);
    } else {
        chargeState = chargeStateDelta(packV);
    }

    // Balancing
    float avg = (c1 + c2 + c3)/3.0f;
    if (relayOn && !ov && !uv && !temp_bad && chargeState == "CHARGING") {
        setBleed(0, c1 > avg + BALANCE_THRESHOLD);
        setBleed(1, c2 > avg + BALANCE_THRESHOLD);
        setBleed(2, c3 > avg + BALANCE_THRESHOLD);
    } else {
        setBleed(0,false); setBleed(1,false); setBleed(2,false);
    }

    // ---------- SERIAL OUTPUT ----------
    Serial.print("Pack "); Serial.print(packV,3);
    Serial.print(" | C1 "); Serial.print(c1,3);
    Serial.print(" C2 "); Serial.print(c2,3);
    Serial.print(" C3 "); Serial.print(c3,3);

    Serial.print(" | Temp "); Serial.print(tempC,1); Serial.print("C");

    Serial.print(" | Relay "); Serial.print(relayOn ? "ON" : "OFF");

    Serial.print(" | Bal ");
    Serial.print(digitalRead(PIN_BLEED[0]) ? "1":"0");
    Serial.print(digitalRead(PIN_BLEED[1]) ? "1":"0");
    Serial.print(digitalRead(PIN_BLEED[2]) ? "1":"0");

    Serial.print(" | State "); Serial.print(chargeState);

    if (ina_present) {
        Serial.print(" | I "); Serial.print(current_mA,1); Serial.print("mA");
    } else {
        Serial.print(" | I N/A");
    }

    if (ov) Serial.print(" | FAULT:OV");
    if (uv) Serial.print(" | FAULT:UV");
    if (temp_bad) Serial.print(" | FAULT:TEMP");

    Serial.println();

    delay(300);
}
