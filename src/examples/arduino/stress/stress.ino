#include "looker.h"
#include "wifi.h"

#define DOMAIN "arduino"
#define DEFAULT_STYLE "margin:0;"

unsigned long j,i = 0;
char k[16] = "empty";

#include "stubs/looker_stubs.c"

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    delay(500);     //skip messages from WiFi module during boot up 
    
    Serial1.begin(115200);
    
    // Turn LED on while connecting
    digitalWrite(LED_BUILTIN, 1);

    looker_init(SSID, PASS, DOMAIN);

    // Turn LED off afterwards
    digitalWrite(LED_BUILTIN, 0);

    looker_reg("i", &i, sizeof(i), LOOKER_VAR_UINT, LOOKER_HTML_VIEW, DEFAULT_STYLE);
    looker_reg("j", &j, sizeof(j), LOOKER_VAR_UINT, LOOKER_HTML_CHECKBOX, DEFAULT_STYLE);
    looker_reg("k", &k, sizeof(k), LOOKER_VAR_STRING, LOOKER_HTML_VIEW, DEFAULT_STYLE);
}

void loop() {
    i++;
    digitalWrite(LED_BUILTIN, j);
    looker_update();
}

