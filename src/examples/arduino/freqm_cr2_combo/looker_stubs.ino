#define LOOKER_STUBS_C
#include "looker_stubs.h"

#define BAUD_RATE 57600

#ifdef DEBUG
#include <SoftwareSerial.h>
SoftwareSerial mySerial(SW_SERIAL_UNUSED_PIN, 4);  //RX, TX   TX is only used, 4 = D2 in NodeMcu
#endif //DEBUG

void serial_init(void)
{
    Serial.begin(57600);
#ifdef DEBUG
    mySerial.begin(57600);
#endif //DEBUG
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
    int n;
    char c;
    for (n = 0; n < size; n++)
    {
        c =  *((char *)buf);
        buf = buf + 1;
        Serial.write((byte) c);
        delay(10);
    }
}
#ifdef DEBUG
void debug_print(const char *s)
{
    mySerial.print(s);
}

void debug_println2(const char *s, int i)
{
    mySerial.print(s);
    mySerial.println(i);
}
#endif //DEBUG

