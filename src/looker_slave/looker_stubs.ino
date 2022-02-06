#define LOOKER_STUBS_C
#include "looker_stubs.h"

#define LOOKER_BAUD_RATE 57600

#ifdef DEBUG
#include <SoftwareSerial.h>
#define DEBUG_BAUD_RATE 57600
#define DEBUG_RX_PIN SW_SERIAL_UNUSED_PIN
#define DEBUG_TX_PIN 4
//4 = D2 in NodeMcu

SoftwareSerial debug_serial(DEBUG_RX_PIN, DEBUG_TX_PIN);
#endif //DEBUG

int looker_stubs_init(void *p)
{
    Serial.begin(LOOKER_BAUD_RATE);
#ifdef DEBUG
    debug_serial.begin(DEBUG_BAUD_RATE);
#endif //DEBUG
    return 0;
}

void looker_delay_1ms(void)
{
    delay(1);
}

size_t looker_data_available(void)
{
    return Serial.available();
}

int looker_get(void *buf, int size)   //non blocking read
{
    int num = 0;

    while ((num < size) && (Serial.available() > 0))
    {
        *((char *)buf) = (char) Serial.read();
        buf = buf + 1;
        num++;
    }
    return num;
}

void looker_send(void *buf, int size)
{
    Serial.write((const uint8_t *) buf, size);
}

#ifdef DEBUG
void debug_print(const char *s)
{
    debug_serial.print(s);
}

void debug_println2(const char *s, int i)
{
    debug_serial.print(s);
    debug_serial.println(i);
}
#endif //DEBUG

