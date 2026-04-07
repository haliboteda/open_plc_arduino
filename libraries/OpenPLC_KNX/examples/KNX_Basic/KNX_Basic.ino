/*
 * KNX_Basic — Minimal OpenPLC_KNX usage example.
 *
 * Demonstrates:
 *   - Initializing KNX TP transport (STKNX via USART1)
 *   - Registering group objects with write callbacks
 *   - Sending a GroupValue.Write on a timer
 *   - Reading back DPT-1, DPT-5, and DPT-9 values in callbacks
 *
 * Hardware: OpenPLC board with STM32H743, STKNX connected to KNX bus.
 *
 * Group address assignments used in this sketch (adjust to your ETS project):
 *   0/0/1  DPT-1.001  Switch output — controls DOUT_1
 *   0/0/2  DPT-5.001  Dimmer level 0-255 — controls PWM on DOUT_5
 *   0/0/3  DPT-9.001  Temperature setpoint (°C)
 */

#include <OpenPLC_KNX.h>

/* KNX individual address of this device — must match ETS project */
static const KnxIndividualAddr MY_ADDR = knxIA(1, 1, 5);

/* Group addresses */
static const KnxGroupAddr GA_SWITCH   = knxGA(0, 0, 1);
static const KnxGroupAddr GA_DIMMER   = knxGA(0, 0, 2);
static const KnxGroupAddr GA_TEMP_SP  = knxGA(0, 0, 3);

/* -------------------------------------------------------------------------
 * Group object callbacks
 * ---------------------------------------------------------------------- */

void onSwitch(const KnxGroupObject *go)
{
    bool state = dpt1_decode(go->value, go->value_len);
    /* Control DOUT_1 (PB13) based on received switch command */
    digitalWrite(DOUT_1, state ? REL_OUTA : REL_OUTB);
}

void onDimmer(const KnxGroupObject *go)
{
    uint8_t level = dpt5_decode(go->value, go->value_len);
    /* Scale 0-255 → PWM 0-255, output on DOUT_5 (PA8, TIM1-CH1) */
    analogWrite(DOUT_5, level);
}

void onTempSetpoint(const KnxGroupObject *go)
{
    float temp_c = dpt9_decode(go->value);
    /* Store or act on temperature setpoint; just print here */
    Serial.print("KNX: temperature setpoint = ");
    Serial.print(temp_c, 2);
    Serial.println(" C");
}

/* -------------------------------------------------------------------------
 * setup()
 * ---------------------------------------------------------------------- */

void setup()
{
    Serial.begin(115200);
    Serial.println("OpenPLC_KNX basic example starting");

    /* Configure digital outputs used by this sketch */
    pinMode(DOUT_1, OUTPUT);
    pinMode(DOUT_5, OUTPUT);
    digitalWrite(DOUT_1, LOW);

    /* Start KNX TP transport */
    KNX.beginTP(MY_ADDR);

    /* Register group objects */
    KNX.addGroupObject(GA_SWITCH,  1, 1, onSwitch);
    KNX.addGroupObject(GA_DIMMER,  5, 1, onDimmer);
    KNX.addGroupObject(GA_TEMP_SP, 9, 1, onTempSetpoint);

    Serial.print("KNX TP initialized. Own address: ");
    Serial.print(knxIA_area(MY_ADDR));
    Serial.print(".");
    Serial.print(knxIA_line(MY_ADDR));
    Serial.print(".");
    Serial.println(knxIA_device(MY_ADDR));
}

/* -------------------------------------------------------------------------
 * loop()
 * ---------------------------------------------------------------------- */

static uint32_t last_send_ms = 0u;

void loop()
{
    /* Required: drive the KNX receive engine and programming mode FSM */
    KNX.process();

    /* Every 10 seconds: broadcast current temperature setpoint */
    if ((millis() - last_send_ms) >= 10000u) {
        last_send_ms = millis();

        float temp_sp = 21.5f;
        if (KNX.groupWrite(GA_TEMP_SP, temp_sp)) {
            Serial.print("KNX: sent temp setpoint ");
            Serial.println(temp_sp);
        }

        /* Print bus status */
        Serial.print("Bus OK: ");
        Serial.print(KNX.tpBusOk() ? "YES" : "NO");
        Serial.print("  Prog mode: ");
        Serial.println(KNX.isProgMode() ? "ON" : "OFF");
    }
}
