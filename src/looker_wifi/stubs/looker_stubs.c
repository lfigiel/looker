void looker_delay(void)
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

void serial_init(void)
{
    Serial.begin(57600);
}

