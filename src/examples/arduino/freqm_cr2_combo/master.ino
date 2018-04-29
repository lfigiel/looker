#include "wifi.h"

#define DEFAULT_STYLE "color:red;font-size:500%;margin:0;"

//globals
volatile char f_str[12];
const char *style = DEFAULT_STYLE;

void master_setup(void)
{
    Serial.begin(115200);
    looker_connect(LOOKER_SSID, LOOKER_PASS, "freqm");
    looker_reg("F", &f_str, 0, LOOKER_TYPE_STRING, LOOKER_LABEL_VIEW, &style);
}

void master_loop(void)
{
    static unsigned char cnt = 0;
    static unsigned long int f = 0;

    while ((cnt < 4) && (Serial.available() > 0))
    {
        char tx_data = (char) Serial.read();
        if (!cnt)
        {
            if (tx_data == 0xFF)
                cnt = 1;
        }
        else
        {
            f += ((unsigned long int) tx_data) << (8*(cnt-1));
            cnt++;
        }
    }

    if (cnt == 4)
    {
        sprintf((char *) f_str, "%lx.%03lx MHz", f >> 12, f & 0xFFF);

        while (Serial.available() > 0)
            Serial.read();
        f = 0;
        cnt = 0;
    }

    looker_update();
}

