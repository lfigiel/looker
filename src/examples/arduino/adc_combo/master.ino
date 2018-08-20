#include "wifi.h"

#define LOOKER_DOMAIN "adc"

//globals
volatile unsigned int adc;
volatile float batt;

void master_setup(void)
{
    looker_connect(LOOKER_SSID, LOOKER_PASS, LOOKER_DOMAIN);
    looker_reg("ADC", &adc, sizeof(adc), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, NULL);
    looker_reg("Batt [V]", &batt, sizeof(batt), LOOKER_TYPE_FLOAT_1, LOOKER_LABEL_VIEW, NULL);
}

void master_loop(void)
{
    adc = analogRead(A0);
    batt = adc * 0.0069;
    looker_update();
    delay(100);
}

