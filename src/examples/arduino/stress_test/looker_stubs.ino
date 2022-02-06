#define LOOKER_STUBS_C
#include <SoftwareSerial.h>
#include "looker_stubs.h"

#define LOOKER_SERIAL_BAUD_RATE 57600
//socket D7 on arduino shield
#define LOOKER_SERIAL_RX_PIN 8
#define LOOKER_SERIAL_TX_PIN 7

#ifdef DEBUG
#define DEBUG_SERIAL_BAUD_RATE 57600
#endif //DEBUG

SoftwareSerial looker_serial(LOOKER_SERIAL_RX_PIN, LOOKER_SERIAL_TX_PIN);

int looker_stubs_init(void *p)
{
    pinMode(LOOKER_SERIAL_RX_PIN, INPUT_PULLUP);
    looker_serial.begin(LOOKER_SERIAL_BAUD_RATE);
#ifdef DEBUG
    Serial.begin(DEBUG_SERIAL_BAUD_RATE);
#endif //DEBUG
    return 0;
}

void looker_delay_1ms(void)
{
    delay(1);   //1ms
}

size_t looker_data_available(void)
{
    return looker_serial.available();
}

int looker_get(void *buf, int size)   //non blocking read
{
    int num = 0;

    while ((num < size) && (looker_serial.available() > 0))
    {
        *((char *)buf) = (char) looker_serial.read();
        buf = buf + 1;
        num++;
    }
    return num;
}

void looker_send(void *buf, int size)
{
    looker_serial.write((const uint8_t *) buf, size);
}

#ifdef DEBUG
void debug_print(const char *s)
{
    Serial.print(s);
}

void debug_println2(const char *s, int i)
{
    Serial.print(s);
    Serial.println(i);
}
#endif //DEBUG
