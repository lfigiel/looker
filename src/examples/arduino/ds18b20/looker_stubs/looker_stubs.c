#define LOOKER_STUBS_C

#define SOFTWARE_SERIAL

#ifdef SOFTWARE_SERIAL
#include <SoftwareSerial.h>
SoftwareSerial mySerial(10, 11);  //RX, TX
#endif //SOFTWARE_SERIAL


void serial_init(void)
{
#ifdef SOFTWARE_SERIAL
    mySerial.begin(57600);
    pinMode(10, INPUT_PULLUP);
#else
    Serial1.begin(57600);
    pinMode(0, INPUT_PULLUP);
#endif //SOFTWARE_SERIAL
}

void looker_delay_1ms(void)
{
    delay(1);   //1ms
}

size_t looker_data_available(void)
{
#ifdef SOFTWARE_SERIAL
    return mySerial.available();
#else
    return Serial1.available();
#endif //SOFTWARE_SERIAL
}

int looker_get(void *buf, int size)   //non blocking read
{
    int num = 0;

#ifdef SOFTWARE_SERIAL
    while ((num < size) && (mySerial.available() > 0))
    {
        *((char *)buf) = (char) mySerial.read();
#else
    while ((num < size) && (Serial1.available() > 0))
    {
        *((char *)buf) = (char) Serial1.read();
#endif //SOFTWARE_SERIAL
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
#ifdef SOFTWARE_SERIAL
        mySerial.write((byte) c);
#else
        Serial1.write((byte) c);
#endif //SOFTWARE_SERIAL
        delay(10);
    }
}

