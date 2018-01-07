#include "looker.h"
#include "wifi.h"

#define DOMAIN "arduino"

unsigned int adc;
unsigned int level = 1024/2;

unsigned char led = 0;
const char *critical_style = "color:red;";
char style[64] = "";

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

    looker_reg("ADC", &adc, sizeof(adc), LOOKER_VAR_UINT, LOOKER_HTML_VIEW, style);
    looker_reg("LEVEL", &level, sizeof(level), LOOKER_VAR_UINT, LOOKER_HTML_EDIT, NULL);
    looker_reg("LED", &led, sizeof(led), LOOKER_VAR_UINT, LOOKER_HTML_CHECKBOX, NULL);
}

void loop() {
    adc = analogRead(A0);

    if (adc < level)
        strcpy(style, "");
    else
        strcpy(style, critical_style);

    digitalWrite(LED_BUILTIN, led);

    looker_update();

    delay(100);
}

