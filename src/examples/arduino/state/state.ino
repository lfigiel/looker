#include "looker.h"
#include "wifi.h"
#include "looker_stubs.h"
#include "looker_stubs/looker_stubs.c"

#define LOOKER_DOMAIN "arduino"

//globals
volatile unsigned int i;
unsigned char led = 0;
looker_slave_state_t slave_state = LOOKER_SLAVE_STATE_RESETING;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, 1);
    delay(500);
    serial_init();

    looker_init(LOOKER_SSID, LOOKER_PASS, LOOKER_DOMAIN);
    looker_reg("i", &i, sizeof(i), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, NULL);
}

void loop() {
    //slave state
    //LED:
    //  solid - not connected
    //  alternate - connecting
    //  off - connected
    slave_state = looker_slave_state();
    if (slave_state == LOOKER_SLAVE_STATE_CONNECTED)
        led = 0;
    else if (slave_state == LOOKER_SLAVE_STATE_CONNECTING)
        led ^= 1;
    else if (slave_state == LOOKER_SLAVE_STATE_RESETING)
        led = 1;
    
    digitalWrite(LED_BUILTIN, led);

    //index
    i++;

    looker_update();
}

