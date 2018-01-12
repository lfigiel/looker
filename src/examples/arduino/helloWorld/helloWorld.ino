#define DOMAIN "arduino"
#include "looker.h"
#include "wifi.h"
#include "stubs/looker_stubs.c"

unsigned int adc;
unsigned char led = 0;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    delay(500);     //skip messages from WiFi module during boot up 
    Serial1.begin(115200);
    looker_init(LOOKER_SSID, LOOKER_PASS, DOMAIN);

    looker_reg("ADC", &adc, sizeof(adc), LOOKER_VAR_UINT, LOOKER_HTML_VIEW, NULL);
    looker_reg("LED", &led, sizeof(led), LOOKER_VAR_UINT, LOOKER_HTML_CHECKBOX, NULL);
}

void loop() {
    adc = analogRead(A0);
    digitalWrite(LED_BUILTIN, led);
    looker_update();
    delay(100);
}

