#define LOOKER_STUBS_C
#include "looker_stubs.h"

#define BAUD_RATE 57600

#include <SoftwareSerial.h>
SoftwareSerial mySerial(10, 11);  //RX, TX

void serial_init(void)
{
    mySerial.begin(BAUD_RATE);
    pinMode(10, INPUT_PULLUP);

#ifdef DEBUG
    Serial.begin(BAUD_RATE);
#endif //DEBUG
}

void looker_delay_1ms(void)
{
    delay(1);   //1ms
}

size_t looker_data_available(void)
{
    return mySerial.available();
}

int looker_get(void *buf, int size)   //non blocking read
{
    int num = 0;

    while ((num < size) && (mySerial.available() > 0))
    {
        *((char *)buf) = (char) mySerial.read();
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
        mySerial.write((byte) c);
        delay(10);
    }
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

