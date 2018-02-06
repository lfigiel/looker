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
#include "checksum.h"

#define LED_ON digitalWrite(ledPin, LOW)
#define LED_OFF digitalWrite(ledPin, HIGH)
#define LED_TOGGLE digitalWrite(ledPin, !digitalRead(ledPin))

#define MSG_TIMEOUT 10     //in ms
#define WIFI_TIMEOUT 20    //in s
#define MS_TIMEOUT 2000    //in ms
#define MS_TIMEOUT_MIN (MS_TIMEOUT-1000000) //in ms
#define MSG_NEW msg.pos = POS_PREFIX

typedef struct {
    char name[LOOKER_VAR_NAME_SIZE];
    char value[LOOKER_VAR_VALUE_SIZE];
    unsigned char size;
    unsigned char type;
    looker_label_t label;
    char style[LOOKER_VAR_STYLE_SIZE];
} var_t;

//globals
looker_slave_state_t slave_state = LOOKER_SLAVE_STATE_UNKNOWN;
var_t var[LOOKER_WIFI_VAR_COUNT_MAX];
size_t var_cnt;      //total variables count
unsigned char http_request;
unsigned char page_refresh;
unsigned char form_submitted;
unsigned char debug=0;

size_t msg_timeout;
int ms_timeout = MS_TIMEOUT;

size_t stat_s_loops;
size_t stat_s_loops_old;
size_t stat_s_loops_s;
size_t stat_ms_updates;
size_t stat_sm_updates;
size_t stat_s_resets;
size_t stat_msg_timeouts;
size_t stat_ms_timeouts;
size_t stat_msg_prefix_errors;
size_t stat_msg_payload_errors;
size_t stat_msg_checksum_errors;
size_t ticker_cnt;

char *ssid = NULL;
char *pass = NULL;
char *domain = NULL;

int ledPin = 2; // GPIO2 on ESP8266
size_t led_cnt, led_timeout;
ESP8266WebServer server(80);
MDNSResponder mdns;
IPAddress ip_addr;
String webString="";
Ticker ticker;

#define TIMEOUT_CNT 100
#include "stubs/looker_stubs.h"
#include "stubs/looker_stubs.c"
#include "msg.h"

//prototypes
void print_u64(char *v, unsigned long long i);
void print_i64(char *v, long long i);
long long atoll(const char *s);
unsigned long long atoull(const char *s);
void print_html(void);

void stat_print(String string, size_t stat, unsigned char red)
{
    webString += "        ";

    if (red && stat)
        webString += "<p style='margin:0;color:red;'>";

    webString += string + ": " + String(stat);

    if (red && stat)
        webString += "</p>\n";
    else
        webString += "<br>\n";
}

void led_period(size_t timeout)
{
    led_timeout = timeout;
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

void print_html(void)
{
    webString =
    "<!doctype html>\n"
    "    <head>\n"
    "        <title>Looker</title>\n";

    webString += "    </head>\n";
    webString += "    <body>\n";

    if (domain)
        webString += "        <h2>" + String(domain) + "</h2>\n";

    //refresh
    webString += "        <form>\n";
    webString += "            <input type='hidden' name='looker_page_refresh'";
    if (page_refresh)
        webString += " value='0'>\n            <input type='submit' value='Turn refresh OFF'><br>\n";
    else
        webString += " value='1'>\n            <input type='submit' value='Turn refresh ON'><br>\n";
    webString += "        </form><br>\n";

    if (ms_timeout >= 0)
    {
        webString += "        <form>\n";

        for (int i=0; i<var_cnt; i++)
        {
            unsigned long long value_int = 0;
            char value_text[(LOOKER_WIFI_VAR_VALUE_SIZE > 20) ? (LOOKER_WIFI_VAR_VALUE_SIZE + 1) : (20 + 1)]; //20 is the length of max 64-bit number

            if ((var[i].label == LOOKER_LABEL_SSID) || (var[i].label == LOOKER_LABEL_PASS) || (var[i].label == LOOKER_LABEL_DOMAIN))
                continue;

            if (var[i].type == LOOKER_TYPE_INT)
            {
                memcpy(&value_int, var[i].value, var[i].size);
                if (var[i].size == 8)
                    print_i64(value_text, (long long) value_int);
                else
                {
                    if (value_int & (1LU << ((8 * var[i].size) - 1)))
                        value_int = -~value_int-1;
                    sprintf(value_text, "%ld", (long long) value_int);
                }
            }
            else if (var[i].type == LOOKER_TYPE_UINT)
            {
                memcpy(&value_int, var[i].value, var[i].size);
                if (var[i].size == 8)
                    print_u64(value_text, value_int);
                else 
                    sprintf(value_text, "%lu", value_int);
            }
            else if (var[i].type == LOOKER_TYPE_STRING)
                strcpy(value_text, var[i].value);
            else if ((var[i].type >= LOOKER_TYPE_FLOAT_0) && (var[i].type <= LOOKER_TYPE_FLOAT_4))
            {
                unsigned char prec = var[i].type - LOOKER_TYPE_FLOAT_0;
                if (var[i].size == sizeof(float))
                {
                    float f;
                    memcpy(&f, var[i].value, sizeof(f));
                    dtostrf(f, 1, prec, value_text);
                }
                else if (var[i].size == sizeof(double))
                {
                    double d;
                    memcpy(&d, var[i].value, sizeof(d));
                    dtostrf(d, 1, prec, value_text);
                }
            }

            webString += "            ";
            if (strlen(var[i].style))
            {
                webString += "<p style='";
                webString += var[i].style;
                webString += "'>";
            }
            else
                webString += "<p>";
            webString += var[i].name;
            webString += ": ";

            if (var[i].label == LOOKER_LABEL_VIEW)
                webString += value_text;
            else if (var[i].label == LOOKER_LABEL_EDIT)
            {
                webString += "<input ";
                if (strlen(var[i].style))
                {
                    webString += "style='";
                    webString += var[i].style;
                    webString += "' ";
                }
                webString += "type='text' name='";
                webString += var[i].name;
                webString += "' value='";
                webString += value_text;
                webString += "'>";
            }
            else if (var[i].label == LOOKER_LABEL_CHECKBOX)
            {
                webString += "<input type='hidden' name='";
                webString += var[i].name;
                webString += "' value='0'>\n";

                webString += "            ";
                webString += "<input type='checkbox' name='";
                webString += var[i].name;
                webString += "' value='1'";
                if (value_int)
                    webString += "checked";
                webString += ">";
            }
            else if (var[i].label == LOOKER_LABEL_CHECKBOX_INV)
            {
                webString += "<input type='hidden' name='";
                webString += var[i].name;
                webString += "' value='1'>\n";

                webString += "            ";
                webString += "<input type='checkbox' name='";
                webString += var[i].name;
                webString += "' value='0'";
                if (!value_int)
                    webString += "checked";
                webString += ">";
            }

            webString += "</p>\n";
        }

        if (!var_cnt)
            webString += "No Variables<br>\n";
        else
            webString += "            <br><input type='submit' value='Submit'><br>\n";

        webString += "        </form>\n";
    }
    else
        webString += "        <h3>Error: Master->Slave Timeout: " + String((MS_TIMEOUT-ms_timeout)/1000) + " s</h3>\n";

    if (debug)
    {
        webString += "        <h2>Debug:</h2>\n";

        stat_print("S Loops", stat_s_loops, 0);
        stat_print("S Loops/s", stat_s_loops_s, 0);
        stat_print("S Resets", stat_s_resets, 0);
        stat_print("MS Updates", stat_ms_updates, 0);
        stat_print("MS Timeouts", stat_ms_timeouts, 1);
        stat_print("MS ACK Failures", stat_ack_get_failures, 1);
        stat_print("SM Updates", stat_sm_updates, 0);
        stat_print("SM ACK Failures", stat_ack_send_failures, 1);
        stat_print("Msg Timeouts", stat_msg_timeouts, 1);
        stat_print("Msg Prefix Errors", stat_msg_prefix_errors, 1);
        stat_print("Msg Payload Errors", stat_msg_payload_errors, 1);
        stat_print("Msg Checksum Errors", stat_msg_checksum_errors, 1);
    }

    if (page_refresh)
    {
        webString += "        <script>\n";
        webString += "            window.setTimeout(function(){window.location.href=window.location.href.split('?')[0]},1000);\n";
        webString += "        </script>\n";
    }

    webString += "    </body>\n";
    webString += "</html>\n";

    server.send(200, "text/html", webString);
}

void ticker_handler()
{
    if (msg_timeout)
        if (!--msg_timeout)
        {
            MSG_NEW;
            stat_msg_timeouts++;
        }

    if (ms_timeout > MS_TIMEOUT_MIN)
        if (--ms_timeout == 0)
        {
            if (!looker_data_available())
                stat_ms_timeouts++;
        }
    if (led_timeout)
        if (++led_cnt >= led_timeout)
        {
            LED_TOGGLE;
            led_cnt = 0;
        }

    if (++ticker_cnt >= 1000)
    {
        stat_s_loops_s = stat_s_loops - stat_s_loops_old;
        stat_s_loops_old = stat_s_loops;
        ticker_cnt = 0;
    }
}

void handle_root()
{
    if (server.args())
    {
        if (strcmp(server.argName(0).c_str(), "looker_page_refresh") == 0)
            page_refresh = (unsigned char) strtoul(server.arg(0).c_str(), NULL, 10);
        else if (strcmp(server.argName(0).c_str(), "looker_debug") == 0)
        {
            debug = (unsigned char) strtoul(server.arg(0).c_str(), NULL, 10);
            if (!debug)
            {
                stat_s_loops = 0;
                stat_s_loops_old = 0;
                stat_s_loops_s = 0;
                stat_ms_updates = 0;
                stat_sm_updates = 0;
                stat_s_resets = 0;
                stat_ack_get_failures = 0;
                stat_ack_send_failures = 0;
                stat_msg_timeouts = 0;
                stat_ms_timeouts = 0;
                stat_msg_prefix_errors = 0;
                stat_msg_payload_errors = 0;
                stat_msg_checksum_errors = 0;
            }
        }
        else
//todo: copy args to buffer - will help with multiple http requests
            form_submitted = 1;
    }
    http_request = 1;
}
 
looker_exit_t sm_update(size_t i)
{
    size_t j;
    unsigned char ack;

    msg.payload_size = 0;
    msg.payload[msg.payload_size++] = COMMAND_UPDATE_VALUE;
    msg.payload[msg.payload_size++] = i;

    memcpy(&msg.payload[msg.payload_size], var[i].value, var[i].size);
    msg.payload_size += var[i].size;

    do
    {
        msg_send();

        //wait for ack
        for (j=0; j<TIMEOUT_CNT; j++)
        {
            looker_delay();
            if (looker_data_available())
            {
                ack = ack_get();
                break;
            }
            else
                ack = ACK_FAILURE; 
        }

        //timeout
        if (j >= TIMEOUT_CNT)
            return LOOKER_EXIT_TIMEOUT;

    } while (ack != ACK_SUCCESS);

    return LOOKER_EXIT_SUCCESS;
}

void var_process(const char *var_name, const char *var_value)
{
    if (!var_cnt)
        return; 

    //find
    int i = 0;
    while ((i < var_cnt) && (strcmp(var[i].name, var_name) != 0))
        i++;

    if (i == var_cnt)
        return;

    //replace if different
    switch (var[i].type) {
        case LOOKER_TYPE_INT:
        {
            long long value_new, value_old;
            value_new = atoll(var_value);
            value_old = 0;
            memcpy(&value_old, var[i].value, var[i].size);

            if (value_new != value_old)
            {
                memcpy(var[i].value, &value_new, var[i].size);
                sm_update(i);
                return;
            }
        break;
        }
        case LOOKER_TYPE_UINT:
        {
            unsigned long long value_new, value_old;
            value_new = atoull(var_value);
            value_old = 0;
            memcpy(&value_old, var[i].value, var[i].size);

            if (value_new != value_old)
            {
                memcpy(var[i].value, &value_new, var[i].size);
                sm_update(i);
                return;
            }
        break;
        }
        case LOOKER_TYPE_FLOAT_0:
        case LOOKER_TYPE_FLOAT_1:
        case LOOKER_TYPE_FLOAT_2:
        case LOOKER_TYPE_FLOAT_3:
        case LOOKER_TYPE_FLOAT_4:
        {
            double value_new, value_old;
            float f;

            value_new = atof(var_value);
            value_old = 0;
            if (var[i].size == sizeof(float))
            {
                memcpy(&f, var[i].value, var[i].size);
                value_old = (double) f;
            }
            else
                memcpy(&value_old, var[i].value, var[i].size);

            if (value_new != value_old)
            {
                if (var[i].size == sizeof(float))
                {
                    f = (float) value_new;
                    memcpy(var[i].value, &f, var[i].size);
                }
                else
                    memcpy(var[i].value, &value_new, var[i].size);

                sm_update(i);
                return;
            }
        break;
        }
        case LOOKER_TYPE_STRING:
            if (strcmp(var[i].value, var_value) != 0)
            {
                strcpy(var[i].value, var_value);
                var[i].size = strlen(var[i].value) + 1;
                sm_update(i);
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
    led_period(0);

    serial_init();
}

static void register_process(void)
{
    size_t i = 1;

    //only new variable
    if (msg.payload[i++] != var_cnt)
        return;

    var[var_cnt].size = msg.payload[i++];
    var[var_cnt].type = msg.payload[i++];

    memcpy(&var[var_cnt].value, &msg.payload[i], var[var_cnt].size);
    i += var[var_cnt].size;

    strcpy(var[var_cnt].name, (char *) &msg.payload[i]);
    i += strlen(var[var_cnt].name) + 1;

    var[var_cnt].label = (looker_label_t) msg.payload[i++];

    strcpy(var[var_cnt].style, (char *) &msg.payload[i]);
    i += strlen(var[var_cnt].style) + 1;

    if (var[var_cnt].label == LOOKER_LABEL_SSID)
        ssid = (char *) &var[var_cnt].value;
    else if (var[var_cnt].label == LOOKER_LABEL_PASS)
        pass = (char *) &var[var_cnt].value;
    if (var[var_cnt].label == LOOKER_LABEL_DOMAIN)
        domain = (char *) &var[var_cnt].value;

    var_cnt++;
}

static looker_exit_t payload_process(void)
{
    size_t i, j;
    unsigned char ack;

    switch (msg.payload[0]) {

        case COMMAND_RESET:
            var_cnt = 0;

            if (WiFi.status() == WL_CONNECTED)
                slave_state = LOOKER_SLAVE_STATE_CONNECTED;
            else
                slave_state = LOOKER_SLAVE_STATE_DISCONNECTED;
            stat_s_resets++;
            ack_send(ACK_SUCCESS);
        break;
        case COMMAND_CONNECT:
            ack_send(ACK_SUCCESS);
        break;
        case COMMAND_STATUS:
            ack_send(ACK_SUCCESS);
        break;
        case COMMAND_REGISTER:
            register_process();
            stat_ms_updates++;
            ack_send(ACK_SUCCESS);
        break;
        case COMMAND_UPDATE_VALUE:
            memcpy(var[msg.payload[1]].value, &msg.payload[2], var[msg.payload[1]].size);
            stat_ms_updates++;
            ack_send(ACK_SUCCESS);
        break;
        case COMMAND_UPDATE_STYLE:
            memcpy(var[msg.payload[1]].style, &msg.payload[2], var[msg.payload[1]].size);
            stat_ms_updates++;
            ack_send(ACK_SUCCESS);
        break;
        case COMMAND_UPDATE_GET:
            ack_send(ACK_SUCCESS);

            //vars
            if (form_submitted)
            {
                for (i=0; i<server.args(); i++)
                {
                    //process only last variable with same name
                    j = i+1;
                    while ((j<server.args()) && (strcmp(server.argName(i).c_str(), server.argName(j).c_str()) != 0))
                        j++;

                    if (j == server.args())
                    {
                        var_process(server.argName(i).c_str(), server.arg(i).c_str());
                        stat_sm_updates++;
                    }
                }
                form_submitted = 0;
            }

            //status
            do
            {
                msg.payload_size = 0;
                msg.payload[msg.payload_size++] = COMMAND_STATUS;
                msg.payload[msg.payload_size++] = slave_state;

                msg_send();
                
                //wait for ack
                for (j=0; j<TIMEOUT_CNT; j++)
                {
                    looker_delay();
                    if (looker_data_available())
                    {
                        ack = ack_get();
                        break;
                    }
                }

                //timeout
                if (j >= TIMEOUT_CNT)
                {
                    return LOOKER_EXIT_TIMEOUT;
                }
            } while (ack != ACK_SUCCESS);

            //all good
            if (http_request)
            {
                print_html();
                http_request = 0;
            }
                
            stat_s_loops++;
            ms_timeout = MS_TIMEOUT;
        break;

        default:
            return LOOKER_EXIT_BAD_COMMAND;
        break;
    }
    return LOOKER_EXIT_SUCCESS;
}

void loop(void)
{
    //todo: maybe this is slow, maybe global variable is faster
    if (WiFi.status() == WL_CONNECTED)
    {
        if (slave_state == LOOKER_SLAVE_STATE_CONNECTING)
        {
            ip_addr = WiFi.localIP();
            if (domain)
                mdns.begin(domain, ip_addr);
            server.on("/", handle_root);
            server.begin();
            led_period(500);
            slave_state = LOOKER_SLAVE_STATE_CONNECTED;
        }
        else
            server.handleClient();
    }
    else
    {
        if ((slave_state == LOOKER_SLAVE_STATE_DISCONNECTED) && (ssid))
        {
            // Connect to WiFi network
            if (pass) 
                WiFi.begin(ssid, pass);
            else
                WiFi.begin(ssid);
            led_period(80);
            slave_state = LOOKER_SLAVE_STATE_CONNECTING;
        }
        else if (slave_state == LOOKER_SLAVE_STATE_CONNECTED)
          slave_state = LOOKER_SLAVE_STATE_DISCONNECTED;
    }

    if (looker_data_available())
    {
        unsigned char err = msg_get();
        if (!err)
        {
            if (msg_complete())
            {
                msg_timeout = 0;
                payload_process();
            }
        }
        else
        {
            MSG_NEW;
            if (err == 1)
                stat_msg_prefix_errors++;
            else if (err == 2)
                stat_msg_payload_errors++;
            else if (err == 3)
                stat_msg_checksum_errors++;
            
            //empty rx buffer
            unsigned char rx;
            while (looker_data_available())
                looker_get(&rx, 1);
            ack_send(ACK_FAILURE);
        }
    }

    if ((ms_timeout < 0) && (http_request))
    {
        print_html();
        form_submitted = 0;
        http_request = 0;
    }
}

