#include <OneWire.h>
#include <DallasTemperature.h>
#include "looker.h"
#include "wifi.h"

#define DOMAIN "arduino"

// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 2

float temp;
char temp_str[8];

// Setup a oneWire instance to communicate with any OneWire devices 
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
 
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

#include "stubs/looker_stubs.c"

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    delay(500);     //skip messages from WiFi module during boot up 
    
    Serial1.begin(115200);

    sensors.begin();

    // Turn LED on while connecting
    digitalWrite(LED_BUILTIN, 1);

    looker_init(SSID, PASS, DOMAIN);

    // Turn LED off afterwards
    digitalWrite(LED_BUILTIN, 0);

    looker_reg("Temp [C]", temp_str, sizeof(temp_str), LOOKER_VAR_STRING, LOOKER_HTML_VIEW, NULL);
}

void loop() {

    digitalWrite(LED_BUILTIN, 1);
    sensors.requestTemperatures();
    digitalWrite(LED_BUILTIN, 0);
    temp = sensors.getTempCByIndex(0);

    //because sprintf flaot is not supported    
    int a,b;
    a = (int) temp;
    b = temp * 10;
    b %= 10;

    sprintf(temp_str, "%d.%d", a, b);
    looker_update();
    delay(500);
}

