#include "looker_master.h"
#include "looker_stubs.h"
#include "wifi.h"

#define LOOKER_DOMAIN "arduino"

//globals
volatile unsigned int adc;
volatile unsigned int level_critical = 1024/2;

const char *style_normal = "font-size:200%;";
const char *style_critical = "color:red;font-size:200%;";
const char *style = style_normal;

void setup() {
    serial_init();

    looker_connect(LOOKER_SSID, LOOKER_PASS, LOOKER_DOMAIN);
    looker_reg("ADC", &adc, sizeof(adc), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, &style);
    looker_reg("LEVEL", &level_critical, sizeof(level_critical), LOOKER_TYPE_UINT, LOOKER_LABEL_EDIT, NULL);
}

void loop() {
    adc = analogRead(A0);
    if (adc < level_critical)
        style = style_normal;
    else
        style = style_critical;
    looker_update();
}

