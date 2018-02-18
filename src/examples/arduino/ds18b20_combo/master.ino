#include <OneWire.h>
#include <DallasTemperature.h>
#include "wifi.h"

// Data wire is plugged into pin 5
#define ONE_WIRE_BUS 5

//globals
volatile float temp;
size_t i;
//unsigned char looker_debug;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void master_setup(void)
{
    sensors.begin();
    looker_connect(LOOKER_SSID, LOOKER_PASS, "combo");
	looker_reg("i", &i, sizeof(i), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, NULL);
//    looker_reg("looker_debug", &looker_debug, sizeof(looker_debug), LOOKER_TYPE_UINT, LOOKER_LABEL_CHECKBOX, NULL);
    looker_reg("Temp [C]", &temp, sizeof(temp), LOOKER_TYPE_FLOAT_1, LOOKER_LABEL_VIEW, "color:blue;font-size:400%;margin:0;");
}

void master_loop(void)
{
    sensors.requestTemperatures();
    temp = sensors.getTempCByIndex(0);
    delay(100);
    i++;
    looker_update();
}

