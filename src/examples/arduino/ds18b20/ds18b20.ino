#include <OneWire.h>
#include <DallasTemperature.h>
#include "looker_master.h"
#include "looker_stubs.h"
#include "wifi.h"

#define LOOKER_DOMAIN "arduino"

// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 2

//globals
volatile float temp;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
    sensors.begin();
    serial_init();

    looker_connect(LOOKER_SSID, LOOKER_PASS, LOOKER_DOMAIN);
    looker_reg("Temp [C]", &temp, sizeof(temp), LOOKER_TYPE_FLOAT_1, LOOKER_LABEL_VIEW, NULL);
}

void loop() {
    sensors.requestTemperatures();
    temp = sensors.getTempCByIndex(0);
    delay(500);

    looker_update();
}

