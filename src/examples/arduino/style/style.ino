#include "looker_master.h"
#include "wifi.h"
#include "looker_stubs.h"
#include "looker_stubs/looker_stubs.c"

#define LOOKER_DOMAIN "arduino"

//globals
volatile unsigned int adc;
volatile unsigned int level_critical = 1024/2;

const char *style_normal = "";
const char *style_critical = "color:red;";
char style[LOOKER_VAR_STYLE_SIZE] = "";

void setup() {
    delay(500);
    serial_init();

    looker_connect(LOOKER_SSID, LOOKER_PASS, LOOKER_DOMAIN);
    looker_reg("ADC", &adc, sizeof(adc), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, style);
    looker_reg("LEVEL", &level_critical, sizeof(level_critical), LOOKER_TYPE_UINT, LOOKER_LABEL_EDIT, NULL);
}

void loop() {
    adc = analogRead(A0);
    if (adc < level_critical)
        strcpy(style, style_normal);
    else
        strcpy(style, style_critical);
    looker_update();
}

