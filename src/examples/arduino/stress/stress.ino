#include "looker.h"
#include "wifi.h"
#include "looker_stubs.h"
#include "looker_stubs/looker_stubs.c"

#define LOOKER_DOMAIN "arduino"

//globals
unsigned char LED = 0;
volatile unsigned long i,j = 0;
volatile char k[16] = "empty";
looker_slave_state_t slave_state;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, 1);
    delay(1000);
    serial_init();

    looker_init(LOOKER_SSID, LOOKER_PASS, LOOKER_DOMAIN);
    looker_reg("i", &i, sizeof(i), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, NULL);
    looker_reg("j", &j, sizeof(j), LOOKER_TYPE_UINT, LOOKER_LABEL_CHECKBOX, NULL);
    looker_reg("k", &k, sizeof(k), LOOKER_TYPE_STRING, LOOKER_LABEL_VIEW, NULL);
}

void loop() {
    //index
    i++;

    //state
    slave_state = looker_slave_state();
    if (slave_state == LOOKER_SLAVE_STATE_CONNECTED)
        LED = 0;
    else if (slave_state == LOOKER_SLAVE_STATE_CONNECTING)
        LED ^= 1;
    else if (slave_state == LOOKER_SLAVE_STATE_RESETING)
        LED = 1;

    //LED
    digitalWrite(LED_BUILTIN, LED);

    looker_update();
}

