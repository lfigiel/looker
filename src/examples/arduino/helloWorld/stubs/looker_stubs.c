int looker_get(void *buf, int size)   //non blocking read
{
    int num = 0;

    while ((num < size) && (Serial1.available() > 0))
    {
        *((char *)buf) = (char) Serial1.read();
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
        Serial1.write((byte) c);
        delay(10);
    }
}

