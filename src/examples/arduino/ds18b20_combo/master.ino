#include <OneWire.h>
#include <DallasTemperature.h>
#include "wifi.h"

#define LOOKER_DOMAIN "temp"

#define ONE_WIRE_BUS 4 //GPIO4

//globals
volatile float temp;
size_t i;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void master_setup(void)
{
    sensors.begin();
    looker_connect(LOOKER_SSID, LOOKER_PASS, LOOKER_DOMAIN);
    looker_reg("i", &i, sizeof(i), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, NULL);
    looker_reg("Temp [C]", &temp, sizeof(temp), LOOKER_TYPE_FLOAT_1, LOOKER_LABEL_VIEW, NULL);
}

void master_loop(void)
{
    sensors.requestTemperatures();
    temp = sensors.getTempCByIndex(0);
    delay(100);
    i++;
    looker_update();
}

