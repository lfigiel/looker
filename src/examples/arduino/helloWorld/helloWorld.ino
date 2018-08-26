#include "looker_master.h"
#include "looker_stubs.h"
#include "wifi.h"

#define LOOKER_DOMAIN "arduino"

volatile unsigned int adc;
volatile unsigned char led = 0;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, 1);
    serial_init();

    looker_wifi_connect(LOOKER_SSID, LOOKER_PASS, LOOKER_DOMAIN);
    looker_reg("ADC", &adc, sizeof(adc), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, NULL);
    looker_reg("LED", &led, sizeof(led), LOOKER_TYPE_UINT, LOOKER_LABEL_CHECKBOX, NULL);
}

void loop() {
    adc = analogRead(A0);
    digitalWrite(LED_BUILTIN, led);
    looker_update();
}

