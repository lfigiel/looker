#include "looker_master.h"
#include "wifi.h"
#include "looker_stubs.h"
#include "looker_stubs/looker_stubs.c"

#define LOOKER_DOMAIN "arduino"

//globals
volatile unsigned int i;
unsigned char led = 0;
looker_slave_state_t slave_state;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, 1);
    delay(500);
    serial_init();

    looker_connect(LOOKER_SSID, LOOKER_PASS, LOOKER_DOMAIN);
    looker_reg("i", &i, sizeof(i), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, NULL);
}

void loop() {
    //index
    i++;

    //slave state
    //LED:
    //  solid - unknown or disconnected
    //  alternate - connecting
    //  off - connected
    slave_state = looker_slave_state();
    if (slave_state == LOOKER_SLAVE_STATE_CONNECTED)
        led = 0;
    else if (slave_state == LOOKER_SLAVE_STATE_CONNECTING)
        led ^= 1;
    else if ((slave_state == LOOKER_SLAVE_STATE_DISCONNECTED) || (slave_state == LOOKER_SLAVE_STATE_UNKNOWN))
        led = 1;

    //LED
    digitalWrite(LED_BUILTIN, led);

    looker_update();
}

