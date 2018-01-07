/*
Copyright (c) 2018 Lukasz Figiel

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Lukasz Figiel
lfigiel@gmail.com
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Ticker.h>
#include "looker_wifi.h"
#include "looker.h"

#define WIFI_TIMEOUT 20     //in s
#define COMM_TIMEOUT 100    //in ms
#define HS_TIMEOUT 2000     //in ms
#define TIMEOUT_DISABLED -1
#define LED_ON digitalWrite(ledPin, LOW)
#define LED_OFF digitalWrite(ledPin, HIGH)

typedef struct {
    char name[LOOKER_WIFI_VAR_NAME_SIZE];
    char value[LOOKER_WIFI_VAR_VALUE_SIZE];
    unsigned char size;
    unsigned char type;
    unsigned char html;
    char style[LOOKER_WIFI_VAR_STYLE_SIZE];
} looker_wifi_var_struct;

void process(unsigned char c);
void print_html(void);

int ledPin = 2; // GPIO2 on ESP8266
ESP8266WebServer server(80);
MDNSResponder mdns;
String webString="";
looker_wifi_var_struct looker_var[LOOKER_WIFI_VAR_COUNT];
unsigned char looker_var_count = 0;
LOOKER_STATE state = LOOKER_STATE_START;
int local_count = 0;    //delete after testing
int hs = 0; //handshake
char var_ready = 0;
char uploading = 0;
IPAddress ip_addr;
char refresh = 1;   //refresh the page
Ticker ticker;
int comm_timeout = TIMEOUT_DISABLED;
int hs_timeout = TIMEOUT_DISABLED;
unsigned char pos = 0;

char ssid[20];
char pass[20];
char domain[20];
typedef enum {
    setup_ssid,
    setup_pass,
    setup_domain
}   setup_t;
setup_t setup_conn;

//todo: dat
int debug1 = 0;
int debug2 = 0;

void change_state(LOOKER_STATE new_state)
{
    pos = 0;
    switch (new_state) {
        case LOOKER_STATE_START:
            state = new_state;
            comm_timeout = TIMEOUT_DISABLED;
        break;
        case LOOKER_STATE_CONNECT:
        case LOOKER_STATE_REG_SIZE:
        case LOOKER_STATE_REG_TYPE:
        case LOOKER_STATE_REG_VALUE:
        case LOOKER_STATE_REG_NAME:
        case LOOKER_STATE_REG_HTML:
        case LOOKER_STATE_REG_STYLE:
        case LOOKER_STATE_UPDATE_VALUE:
        case LOOKER_STATE_UPDATE_STYLE:
            state = new_state;
            comm_timeout = COMM_TIMEOUT;
        break;
    }

    switch (new_state) {
        case LOOKER_STATE_UPDATE_VALUE:
        case LOOKER_STATE_UPDATE_STYLE:
            uploading = 1;
        break;
    }
}

void process(unsigned char c)
{
    static int var_number;

    if (comm_timeout > 0)
        comm_timeout = COMM_TIMEOUT;

    switch (state) {
        case LOOKER_STATE_START:
            setup_conn = setup_ssid;
            var_number = -1;
            switch (c) {
                case LOOKER_COMM_RESET:
                    looker_var_count = 0;
                break;
                case LOOKER_COMM_CONNECT:
                    change_state(LOOKER_STATE_CONNECT);
                break;
                case LOOKER_COMM_REG:
                    change_state(LOOKER_STATE_REG_SIZE);
                break;
                case LOOKER_COMM_UPDATE_VALUE:
                    change_state(LOOKER_STATE_UPDATE_VALUE);
                break;
                case LOOKER_COMM_UPDATE_STYLE:
                    change_state(LOOKER_STATE_UPDATE_STYLE);
                break;
                case LOOKER_COMM_HANDSHAKE:
                    hs = 1;
                    uploading = 0;
                break;
                default:
                    //bad command
                break;
            }
        break;

        case LOOKER_STATE_CONNECT:
            switch (setup_conn) {
                case setup_ssid:
                    ssid[pos++] = c;
                    if (!c)
                    {
                        setup_conn = setup_pass;
                        pos = 0;
                    }
                break;
                case setup_pass:
                    pass[pos++] = c;
                    if (!c)
                    {
                        setup_conn = setup_domain;
                        pos = 0;
                    }
                break;
                case setup_domain:
                    domain[pos++] = c;
                    if (!c)
                    {
                        Serial.write(setup_wifi());
                        change_state(LOOKER_STATE_START);
                    }
                break;
            }
        break;

        case LOOKER_STATE_REG_SIZE:
            looker_var[looker_var_count].size = c;
            change_state(LOOKER_STATE_REG_TYPE);
        break;

        case LOOKER_STATE_REG_TYPE:
            looker_var[looker_var_count].type = c;
            change_state(LOOKER_STATE_REG_VALUE);
        break;

        case LOOKER_STATE_REG_VALUE:
            looker_var[looker_var_count].value[pos++] = c;
            if (looker_var[looker_var_count].type == LOOKER_VAR_STRING)
            {
                if (!c)
                    change_state(LOOKER_STATE_REG_NAME);
            }
            else        
                if (looker_var[looker_var_count].size == pos)
                    change_state(LOOKER_STATE_REG_NAME);
        break;

        case LOOKER_STATE_REG_NAME:
            looker_var[looker_var_count].name[pos++] = c;
            if (!c)
                change_state(LOOKER_STATE_REG_HTML);
        break;

        case LOOKER_STATE_REG_HTML:
            looker_var[looker_var_count].html = c;
            change_state(LOOKER_STATE_REG_STYLE);
        break;

        case LOOKER_STATE_REG_STYLE:
            looker_var[looker_var_count].style[pos++] = c;
            if (!c)
            {
                looker_var_count++;
                change_state(LOOKER_STATE_START);
            }
        break;

        case LOOKER_STATE_UPDATE_VALUE:
            if (var_number >= 0)
            {
                looker_var[var_number].value[pos++] = c;
                if (looker_var[var_number].type == LOOKER_VAR_STRING)
                {
                    if (!c)
                        change_state(LOOKER_STATE_START);
                }
                else
                    if (looker_var[var_number].size == pos)
                        change_state(LOOKER_STATE_START);
            }
            else
                var_number = c;
        break;

        case LOOKER_STATE_UPDATE_STYLE:
            if (var_number >= 0)
            {
                looker_var[var_number].style[pos++] = c;
                if (!c)
                    change_state(LOOKER_STATE_START);
            }
            else
                var_number = c;
        break;
    }
}

void print_u64(char *v, unsigned long long i)
{
    unsigned int a = i / 1000000000000000000LU;

    if (a)
    {
        sprintf(v, "%u", a);
        v += strlen(v);

        a = (i % 1000000000000000000LU) / 1000000000LU;  
        sprintf(v, "%09u", a);
        v += strlen(v);

        a = i % 1000000000LU;
        sprintf(v, "%09u", a);
    }
    else
    {
        a = (i % 1000000000000000000LU) / 1000000000LU;  
        if (a)
        {
            sprintf(v, "%u", a);
            v += strlen(v);

            a = i % 1000000000LU;
            sprintf(v, "%09u", a);
        }
        else
        {
            a = i % 1000000000LU;
            sprintf(v, "%u", a);
        }
    }
}

void print_i64(char *v, long long i)
{
    if (i<0)
    {
        i = -i;
        strcpy(v++, "-");
    }

    print_u64(v, i);
}

void print_html(void)
{
    webString =
    "<!doctype html>\n"
    "    <head>\n"
    "        <title>Looker</title>\n";

    webString += "    </head>\n";
    webString += "    <body>\n";

    webString += "        <h2>" + String(domain) + "</h2>\n";

    //refresh
    webString += "        <form>\n";
    webString += "            <input type='hidden' name='looker_page_refresh'";
    if (refresh)
        webString += " value='0'>\n            <input type='submit' value='Turn refresh OFF'><br>\n";
    else
        webString += " value='1'>\n            <input type='submit' value='Turn refresh ON'><br>\n";
    webString += "        </form><br>\n";

    
    webString += "        <form>\n";

    for (int i=0; i<looker_var_count; i++)
    {
        unsigned long long value_int = 0;
        char value_text[(LOOKER_WIFI_VAR_VALUE_SIZE > 20) ? (LOOKER_WIFI_VAR_VALUE_SIZE + 1) : (20 + 1)]; //20 is the length of max 64-bit number
        if (looker_var[i].type == LOOKER_VAR_INT)
        {
            memcpy(&value_int, looker_var[i].value, looker_var[i].size);
            if (looker_var[i].size == 8)
                print_i64(value_text, (long long) value_int);
            else
            {
                if (value_int & (1LU << ((8 * looker_var[i].size) - 1)))
                    value_int = -~value_int-1;
                sprintf(value_text, "%ld", (long long) value_int);
            }
        }
        else if (looker_var[i].type == LOOKER_VAR_UINT)
        {
            memcpy(&value_int, looker_var[i].value, looker_var[i].size);
            if (looker_var[i].size == 8)
                print_u64(value_text, value_int);
            else 
                sprintf(value_text, "%lu", value_int);
        }
        else if (looker_var[i].type == LOOKER_VAR_STRING)
            strcpy(value_text, looker_var[i].value);
        else if (looker_var[i].type == LOOKER_VAR_FLOAT)
        {
            if (looker_var[i].size == sizeof(float))
            {
                float f;
                memcpy(&f, looker_var[i].value, sizeof(f));
                dtostrf(f, 1, LOOKER_WIFI_FLOAT_PREC, value_text);
            }
            else if (looker_var[i].size == sizeof(double))
            {
                double d;
                memcpy(&d, looker_var[i].value, sizeof(d));
                dtostrf(d, 1, LOOKER_WIFI_FLOAT_PREC, value_text);
            }
        }

        webString += "            ";
        if (strlen(looker_var[i].style))
        {
            webString += "<p style='";
            webString += looker_var[i].style;
            webString += "'>";
        }
        else
            webString += "<p>";
        webString += looker_var[i].name;
        webString += ": ";

        if (looker_var[i].html == LOOKER_HTML_VIEW)
            webString += value_text;
        else if (looker_var[i].html == LOOKER_HTML_EDIT)
        {
            webString += "<input ";
            if (strlen(looker_var[i].style))
            {
                webString += "style='";
                webString += looker_var[i].style;
                webString += "' ";
            }
            webString += "type='text' name='";
            webString += looker_var[i].name;
            webString += "' value='";
            webString += value_text;
            webString += "'>";
        }
        else if (looker_var[i].html == LOOKER_HTML_CHECKBOX)
        {
            webString += "<input type='hidden' name='";
            webString += looker_var[i].name;
            webString += "' value='0'>\n";

            webString += "            ";
            webString += "<input type='checkbox' name='";
            webString += looker_var[i].name;
            webString += "' value='1'";
            if (value_int)
                webString += "checked";
            webString += ">";
        }
        else if (looker_var[i].html == LOOKER_HTML_CHECKBOX_INV)
        {
            webString += "<input type='hidden' name='";
            webString += looker_var[i].name;
            webString += "' value='1'>\n";

            webString += "            ";
            webString += "<input type='checkbox' name='";
            webString += looker_var[i].name;
            webString += "' value='0'";
            if (!value_int)
                webString += "checked";
            webString += ">";
        }

        webString += "</p>\n";
    }
/*
webString += String(server.args()) + "<br>";
for (int i=0; i<server.args(); i++)
  webString += server.argName(i) + ": " + server.arg(i) + "<br>";
*/   

    if (!looker_var_count)
    {
        webString += "SSID: " + String(ssid) + "<br>\n";
        webString += "LOOKER_WIFI_VAR_COUNT: " + String(LOOKER_WIFI_VAR_COUNT) + "<br>\n";
        webString += "LOOKER_WIFI_VAR_VALUE_SIZE: " + String(LOOKER_WIFI_VAR_COUNT) + "<br>\n";
        webString += "LOOKER_WIFI_VAR_NAME_SIZE: " + String(LOOKER_WIFI_VAR_COUNT) + "<br>\n";
    }
    else
        webString += "            <br><input type='submit' value='Submit'><br>\n";

    webString += "        </form>\n";

    //todo: dat
    //webString += "hs_timeout: " + String(hs_timeout) + "\n";  

    if (hs_timeout == 0)
        webString += "        <h3>Error: Target Sync Timeout !!!</h2>\n";

    if (refresh)
    {
        webString += "        <script>\n";
        webString += "            window.setTimeout(function(){window.location.href=window.location.href.split('?')[0]},1000);\n";
        webString += "        </script>\n";
    }

    webString += "    </body>\n";
    webString += "</html>\n";

    server.send(200, "text/html", webString);
}

void print_debug(void)
{
    String webString2;

    webString2 =
    "local_count: " + String(local_count) + "\n"
    "looker_var_count: " + String(looker_var_count) + "\n"
    "state: " + String(state) + "\n"
    "size: " + String(looker_var[0].size) + "\n"
    "type: " + String(looker_var[0].type) + "\n"
    "html: " + String(looker_var[0].html) + "\n"
    "name: " + looker_var[0].name + "\n"
    "debug1: " + String(debug1) + "\n"
    "debug2: " + String(debug2) + "\n";
    server.send(200, "text/text", webString2);
}

void ticker_handler()
{
    if (comm_timeout > 0)
        comm_timeout--;

    if (hs_timeout > 0)
        hs_timeout--;
}

void handle_root() {
    LED_ON;

    if (server.args())
    {   
        if (strcmp(server.argName(0).c_str(), "looker_page_refresh") == 0)
        //one argument = looker_page_refresh
        {
            refresh = (char) strtoul(server.arg(0).c_str(), NULL, 10);
        }
        else
        //more = vars
        {
            var_ready = 1;
            hs_timeout = HS_TIMEOUT;
            return;
        }
    }

    if (!uploading)
    {
        print_html();
        hs_timeout = TIMEOUT_DISABLED;
        LED_OFF;
    }
    else
        hs_timeout = HS_TIMEOUT;
}
 
LOOKER_EXIT_CODE setup_wifi(void)
{
    if (WiFi.status() != WL_CONNECTED)
    {        
        // Connect to WiFi network
        WiFi.begin(ssid, pass);

        int i = 0;
        // Wait for connection
        do {
            digitalWrite(ledPin, !digitalRead(ledPin));
            delay(100);
            if (++i >= (WIFI_TIMEOUT * 10))
            {
                WiFi.disconnect();
                return LOOKER_EXIT_SERVER_TIMEOUT;
            }
        } while (WiFi.status() != WL_CONNECTED);

        ip_addr = WiFi.localIP();

        if (domain[0])
            mdns.begin(domain, ip_addr);

        server.on("/", handle_root);
        server.begin();
        LED_OFF;
    }
    return LOOKER_EXIT_SUCCESS;
}

long long atoll(const char *s)
{
    long long l = 0;
    int i = 0;
    char minus = 0;

    while (s[i] == ' ')
        i++;

    if (s[i] == '-')
    {
        minus = 1;
        i++;
    }

    l = atoull(&s[i]);

    if (minus)
        return -l;
    else
        return l;
}

unsigned long long atoull(const char *s)
{
    unsigned long long l = 0;
    int i = 0;

    while (s[i] == ' ')
        i++;

    while (s[i])
    {
        l += (s[i++] - '0');
        if (s[i])
            l *= 10;
    }

    return l;
}

void var_process(const char *var_name, const char *var_value)
{
    if (!looker_var_count)
        return; 

    //find
    int i = 0;
    while ((i < looker_var_count) && (strcmp(looker_var[i].name, var_name) != 0))
        i++;

    if (i == looker_var_count)
        return;

    //replace if different
    switch (looker_var[i].type) {
        case LOOKER_VAR_INT:
        {
            long long value_new, value_old;
            value_new = atoll(var_value);
            value_old = 0;
            memcpy(&value_old, looker_var[i].value, looker_var[i].size);

            if (value_new != value_old)
            {
                memcpy(looker_var[i].value, &value_new, looker_var[i].size);
                Serial.write(LOOKER_COMM_UPDATE_VALUE);
                Serial.write(i);
                int size;
                size = looker_var[i].size;
                for (int j=0; j<size; j++)
                    Serial.write(looker_var[i].value[j]);
                return;
            }
        break;
        }
        case LOOKER_VAR_UINT:
        {
            unsigned long long value_new, value_old;
            value_new = atoull(var_value);
            value_old = 0;
            memcpy(&value_old, looker_var[i].value, looker_var[i].size);

            if (value_new != value_old)
            {
                memcpy(looker_var[i].value, &value_new, looker_var[i].size);
                Serial.write(LOOKER_COMM_UPDATE_VALUE);
                Serial.write(i);
                int size;
                size = looker_var[i].size;
                for (int j=0; j<size; j++)
                    Serial.write(looker_var[i].value[j]);
                return;
            }
        break;
        }
        case LOOKER_VAR_FLOAT:
        {
            double value_new, value_old;
            float f;

            value_new = atof(var_value);
            value_old = 0;
            if (looker_var[i].size == sizeof(float))
            {
                memcpy(&f, looker_var[i].value, looker_var[i].size);
                value_old = (double) f;
            }
            else
                memcpy(&value_old, looker_var[i].value, looker_var[i].size);

            if (value_new != value_old)
            {
                if (looker_var[i].size == sizeof(float))
                {
                    f = (float) value_new;
                    memcpy(looker_var[i].value, &f, looker_var[i].size);
                }
                else
                    memcpy(looker_var[i].value, &value_new, looker_var[i].size);

                Serial.write(LOOKER_COMM_UPDATE_VALUE);
                Serial.write(i);
                int size;
                size = looker_var[i].size;
                for (int j=0; j<size; j++)
                    Serial.write(looker_var[i].value[j]);
                return;
            }
        break;
        }
        case LOOKER_VAR_STRING:
            if (strcmp(looker_var[i].value, var_value) != 0)
            {
                strcpy(looker_var[i].value, var_value);
                Serial.write(LOOKER_COMM_UPDATE_VALUE);
                Serial.write(i);
                int size;
                size = strlen(looker_var[i].value) + 1;
                for (int j=0; j<size; j++)
                    Serial.write(looker_var[i].value[j]);
                return;
            }
        break;
        default:
            return;
    }
    return;
}

void setup(void)
{
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);

    ticker.attach_ms(1, ticker_handler);

    pinMode(ledPin, OUTPUT);
    LED_ON;

    Serial.begin(115200);
/*
    if (Serial.available() > 0)
    {
      Serial.read();
      delay(1);
    }
*/
}

void loop(void)
{
    //todo: maybe this is slow, maybe global variable would be faster   
    if (WiFi.status() == WL_CONNECTED)  
        server.handleClient();
 
    if (Serial.available() > 0)
    {
        int serial_data = Serial.read();
        process(serial_data);
        local_count++;
    }

    if (hs)
    {
        if (var_ready)
        {
            //update vars and send locally if different
            for (int i=0; i<server.args(); i++)
            {
                //process only last variable with same name
                int j = i+1;
                while ((j<server.args()) && (strcmp(server.argName(i).c_str(), server.argName(j).c_str()) != 0))
                    j++;

                if (j == server.args())
                    var_process(server.argName(i).c_str(), server.arg(i).c_str());
            }
            
            print_html();
            var_ready = 0;
            hs_timeout = TIMEOUT_DISABLED;
            LED_OFF;
        }
        else if (hs_timeout > 0)
        {
            print_html();
            hs_timeout = TIMEOUT_DISABLED;
            LED_OFF;
        }

        Serial.write(LOOKER_COMM_HANDSHAKE);
        hs = 0;
    }

    if (comm_timeout == 0)
    {
        state = LOOKER_STATE_START;
        comm_timeout = TIMEOUT_DISABLED;
    }

    if (hs_timeout == 0)
    {
        print_html();
//        print_debug();  //todo: dat
        var_ready = 0;
        hs_timeout = TIMEOUT_DISABLED;
        LED_OFF;
    }
}

