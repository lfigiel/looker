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
#include "looker_slave.h"
#include "looker_stubs.h"
#ifndef LOOKER_COMBO
    #include "msg.h"
#endif //LOOKER_COMBO

#define LED_ON digitalWrite(ledPin, LOW)
#define LED_OFF digitalWrite(ledPin, HIGH)
#define LED_TOGGLE digitalWrite(ledPin, !digitalRead(ledPin))

//todo: implement MSG_TIMEOUT
//#define MSG_TIMEOUT 10     //ms
#define WIFI_TIMEOUT 20    //s
#define MS_TIMEOUT 2000    //ms
#define MS_TIMEOUT_MIN (MS_TIMEOUT-1000000) //in ms
#ifndef LOOKER_COMBO
    #define MSG_NEW msg.pos = POS_PREFIX
#endif //LOOKER_COMBO

#ifdef LOOKER_COMBO
typedef struct {
    const char *name;
    void *value_current;
    char value_old[LOOKER_SLAVE_VAR_VALUE_SIZE];
    unsigned char size;
    unsigned char type;
    looker_label_t label;
#if LOOKER_SLAVE_STYLE == LOOKER_STYLE_FIXED
    const char *style_current;
#elif LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE
    const char **style_current;
    const char *style_old;
#endif //LOOKER_SLAVE_STYLE == LOOKER_STYLE_FIXED
    unsigned char style_update;
} var_t;
#else
typedef struct {
    char name[LOOKER_SLAVE_VAR_NAME_SIZE];
    char value_current[LOOKER_SLAVE_VAR_VALUE_SIZE];
    unsigned char size;
    unsigned char type;
    looker_label_t label;
    char style_current[LOOKER_VAR_STYLE_SIZE];
} var_t;
#endif //LOOKER_COMBO

//todo: change vars to static
//todo: add LOOKER_ to var names

//globals
looker_slave_state_t slave_state = LOOKER_SLAVE_STATE_DISCONNECTED;
#ifdef LOOKER_SLAVE_USE_MALLOC
    static var_t *var = NULL;
#else
    static var_t var[LOOKER_SLAVE_VAR_COUNT];
#endif //LOOKER_SLAVE_USE_MALLOC
size_t var_cnt;      //total variables count
unsigned char http_request;
unsigned char form_submitted;
unsigned char *looker_debug_show = NULL;

#ifndef LOOKER_COMBO
    msg_t msg;
    extern size_t stat_ack_get_failures;
    extern size_t stat_ack_send_failures;
#endif //LOOKER_COMBO
size_t msg_timeout;
int ms_timeout = MS_TIMEOUT;
unsigned char rebooted;

size_t stat_s_loops;
size_t stat_s_loops_old;
size_t stat_s_loops_s;
size_t stat_ms_updates;
size_t stat_sm_updates;
size_t stat_s_resets;
size_t stat_ms_timeouts;
#ifndef LOOKER_COMBO
    size_t stat_msg_timeouts;
    size_t stat_msg_prefix_errors;
    size_t stat_msg_payload_errors;
    size_t stat_msg_checksum_errors;
#endif //LOOKER_COMBO
size_t ticker_cnt;

const char *network_ssid = NULL;
const char *network_pass = NULL;
const char *network_domain = NULL;
looker_network_t network = LOOKER_NETWORK_DO_NOTHING;

int ledPin = 2; // GPIO2 on ESP8266
size_t led_cnt, led_timeout;
ESP8266WebServer server(80);
MDNSResponder mdns;
IPAddress ip_addr;
String webString="";
Ticker ticker;

#define TIMEOUT_CNT 100

//prototypes
static void slave_setup(void);
static void slave_loop(void);
void print_u64(char *v, unsigned long long i);
void print_i64(char *v, long long i);
long long atoll(const char *s);
unsigned long long atoull(const char *s);
void print_html(void);
static looker_exit_t network_connect(char *ssid, char *pass);
static void pointers_update(void);

//todo: also static or enough if only in prototype
void pointers_update(void)
{
    size_t i;
    for (i=0; i<var_cnt; i++)
    {
        switch (var[i].label) {
            case LOOKER_LABEL_SSID:
                network_ssid = (char *) &var[i].name;
            break;
            case LOOKER_LABEL_PASS:
                network_pass = (char *) &var[i].name;
            break;
            case LOOKER_LABEL_DOMAIN:
                network_domain = (char *) &var[i].name;
            break;
            default:
            break;
        }

        if (!strcmp(var[i].name, "looker_debug"))
            looker_debug_show = (unsigned char *) &var[i].value_current;
    }
}

looker_exit_t network_connect(const char *ssid, const char *pass)
{
    if (!ssid)
        return LOOKER_EXIT_BAD_PARAMETER;

    PRINT("  ssid: "); PRINT(ssid); PRINT("\n");
    // Connect to WiFi network
    if (pass)
    {
        PRINT("  pass: "); PRINT(pass); PRINT("\n");
        WiFi.begin(ssid, pass);
    }
    else
        WiFi.begin(ssid);
    return LOOKER_EXIT_SUCCESS;
}

#ifdef LOOKER_COMBO
//#include "master.h"

void looker_init(void)
{
    //reset master
    var_cnt = 0;
    network_ssid = NULL;
    network_pass = NULL;
    network_domain = NULL;
}

void looker_destroy(void)
{
#ifdef LOOKER_SLAVE_USE_MALLOC
    if (var)
    {
        free(var);
        var = NULL;
    }
#endif //LOOKER_SLAVE_USE_MALLOC
}

looker_exit_t looker_connect(const char *ssid, const char *pass, const char *domain)
{
    looker_destroy();
    looker_init();

    network_ssid = ssid;
    network_pass = pass;
    network_domain = domain;
    network = LOOKER_NETWORK_RECONNECT;

    return LOOKER_EXIT_SUCCESS;
}

looker_exit_t looker_reg(const char *name, volatile void *addr, int size, looker_type_t type, looker_label_t label, looker_style_t style)
{
    if ((type == LOOKER_TYPE_STRING) && (label != LOOKER_LABEL_SSID) & (label != LOOKER_LABEL_PASS) && (label != LOOKER_LABEL_DOMAIN))
        size = strlen((const char *) addr) + 1;

#ifdef LOOKER_SLAVE_SANITY_TEST
    if (var_cnt >= LOOKER_SLAVE_VAR_COUNT)
        return LOOKER_EXIT_NO_MEMORY;

    if (type >= LOOKER_TYPE_LAST)
        return LOOKER_EXIT_BAD_PARAMETER;

    if (size > LOOKER_SLAVE_VAR_VALUE_SIZE)
        return LOOKER_EXIT_BAD_PARAMETER;

    if ((type >= LOOKER_TYPE_FLOAT_0) && (type <= LOOKER_TYPE_FLOAT_4) && (size != sizeof(float)) && (size != sizeof(double)))
        return LOOKER_EXIT_BAD_PARAMETER;

    if (name && (strlen(name) + 1 > LOOKER_SLAVE_VAR_NAME_SIZE))
        return LOOKER_EXIT_BAD_PARAMETER;

    if (label >= LOOKER_LABEL_LAST)
        return LOOKER_EXIT_BAD_PARAMETER;

#if LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE
    if (style && *style &&(strlen(*style) + 1 > LOOKER_VAR_STYLE_SIZE))
#else
    if (style &&(strlen(style) + 1 > LOOKER_VAR_STYLE_SIZE))
#endif //LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE
        return LOOKER_EXIT_BAD_PARAMETER;

    size_t i;
    //one ssid, one pass, one domain
    if ((label == LOOKER_LABEL_SSID) || (label == LOOKER_LABEL_PASS) || (label == LOOKER_LABEL_DOMAIN))
    {
        //in combo mode ssid, pass and domain are already set up in connect function
        return LOOKER_EXIT_BAD_PARAMETER;
    }
    else
    {
        //name is a must
        if (!name)
            return LOOKER_EXIT_BAD_PARAMETER;

        //name cannot be duplicated
        for (i=0; i<var_cnt; i++)
            if ((var[i].name) && (!strcmp(var[i].name, name)))
                return LOOKER_EXIT_BAD_PARAMETER;
    }
#endif //LOOKER_SLAVE_SANITY_TEST

#ifdef LOOKER_SLAVE_USE_MALLOC
    var_t *p;
    if ((p = (var_t *) realloc(var, (size_t) ((var_cnt + 1) * sizeof(var_t)))) == NULL)
    {
        //looker_destroy() will free old memory block pointed by var
        return LOOKER_EXIT_NO_MEMORY;
    }
    var = p;
    //because memory block might have moved
    pointers_update();
#endif //LOOKER_SLAVE_USE_MALLOC

    var[var_cnt].value_current = (void *) addr;
    var[var_cnt].size = size;
    var[var_cnt].type = type;
    var[var_cnt].label = label;
#if ((LOOKER_SLAVE_STYLE == LOOKER_STYLE_FIXED) || (LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE))
    var[var_cnt].style_current = style;
#endif //((LOOKER_SLAVE_STYLE == LOOKER_STYLE_FIXED) || (LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE))

    var[var_cnt].name = name;

    //set up looker_debug pointer
    //in combo mode ssid, pass and domain are already set up in connect function
    if (!strcmp(var[var_cnt].name, "looker_debug"))
        looker_debug_show = (unsigned char *) var[var_cnt].value_current;

    var_cnt++;

    return LOOKER_EXIT_SUCCESS;
}

looker_exit_t looker_update(void)
{
    ms_timeout = MS_TIMEOUT;
    return LOOKER_EXIT_SUCCESS;
}
#endif //LOOKER_COMBO

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
    "        <title>Looker</title>\n"
    "    </head>\n"
    "    <body>\n";

    if (network_domain)
        webString += "        <h2>" + String(network_domain) + "</h2>\n";

    webString +=
    "        <button id='refresh_id' onclick='refresh_func()'></button>\n"
    "        <script>\n"
    "            var refresh_label = 'Turn refresh OFF'\n"
    "            var refresh_timeout;\n"
    "            if (refresh_label == 'Turn refresh OFF')\n"
    "                refresh_timeout = window.setTimeout(function(){window.location.href=window.location.href.split('?')[0]},1000);\n"
    "            document.getElementById('refresh_id').innerHTML = refresh_label;\n"
    "            function refresh_func() {\n"
    "                if (refresh_label == 'Turn refresh OFF')\n"
    "                {\n"
    "                    clearTimeout(refresh_timeout);\n"
    "                    refresh_label = 'Turn refresh ON';\n"
    "                }\n"
    "                else\n"
    "                    window.location.href=window.location.href.split('?')[0];\n"
    "                document.getElementById('refresh_id').innerHTML = refresh_label;\n"
    "            }\n"
    "        </script>\n";

    if (ms_timeout >= 0)
    {
        webString += "        <form><br>\n";

        for (int i=0; i<var_cnt; i++)
        {
            unsigned long long value_int = 0;
            char value_text[(LOOKER_SLAVE_VAR_VALUE_SIZE > 20) ? (LOOKER_SLAVE_VAR_VALUE_SIZE + 1) : (20 + 1)]; //20 is the length of max 64-bit number

            if ((var[i].label == LOOKER_LABEL_SSID) || (var[i].label == LOOKER_LABEL_PASS) || (var[i].label == LOOKER_LABEL_DOMAIN))
                continue;

            if (var[i].type == LOOKER_TYPE_INT)
            {
                memcpy(&value_int, var[i].value_current, var[i].size);
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
                memcpy(&value_int, var[i].value_current, var[i].size);
                if (var[i].size == 8)
                    print_u64(value_text, value_int);
                else 
                    sprintf(value_text, "%lu", value_int);
            }
            else if (var[i].type == LOOKER_TYPE_STRING)
                strcpy(value_text, (char *) var[i].value_current);
            else if ((var[i].type >= LOOKER_TYPE_FLOAT_0) && (var[i].type <= LOOKER_TYPE_FLOAT_4))
            {
                unsigned char prec = var[i].type - LOOKER_TYPE_FLOAT_0;
                if (var[i].size == sizeof(float))
                {
                    float f;
                    memcpy(&f, var[i].value_current, sizeof(f));
                    dtostrf(f, 1, prec, value_text);
                }
                else if (var[i].size == sizeof(double))
                {
                    double d;
                    memcpy(&d, var[i].value_current, sizeof(d));
                    dtostrf(d, 1, prec, value_text);
                }
            }

            webString += "            ";
#ifdef LOOKER_COMBO
#if LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE
            if ((var[i].style_current) && (*var[i].style_current))
#else
            if (var[i].style_current)
#endif //LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE
#else
            if (strlen(var[i].style_current))
#endif //LOOKER_COMBO
            {
                webString += "<p style='";
//todo: not String(var[i].style) ???
#ifdef LOOKER_COMBO
#if LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE
                webString += *var[i].style_current;
#else
                webString += var[i].style_current;
#endif //LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE
#else
                webString += var[i].style_current;
#endif //LOOKER_COMBO
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
#ifdef LOOKER_COMBO
#if LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE
            if ((var[i].style_current) && (*var[i].style_current))
#else
            if (var[i].style_current)
#endif //LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE
#else
            if (strlen(var[i].style_current))
#endif //LOOKER_COMBO
                {
                    webString += "style='";
//todo: not String(var[i].style) ???
#ifdef LOOKER_COMBO
#if LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE
                webString += *var[i].style_current;
#else
                webString += var[i].style_current;
#endif //LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE
#else
                webString += var[i].style_current;
#endif //LOOKER_COMBO
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

    if ((looker_debug_show) && (*looker_debug_show))
    {
        webString += "        <h2>Debug:</h2>\n";
        stat_print("S State", slave_state, 0);
        stat_print("S Loops", stat_s_loops, 0);
        stat_print("S Loops/s", stat_s_loops_s, 0);
        stat_print("S Resets", stat_s_resets, 0);
        stat_print("MS Updates", stat_ms_updates, 0);
        stat_print("MS Timeouts", stat_ms_timeouts, 1);
#ifndef LOOKER_COMBO
        stat_print("MS ACK Failures", stat_ack_get_failures, 1);
        stat_print("SM ACK Failures", stat_ack_send_failures, 1);
#endif //LOOKER_COMBO
        stat_print("SM Updates", stat_sm_updates, 0);
#ifndef LOOKER_COMBO
        stat_print("Msg Timeouts", stat_msg_timeouts, 1);
        stat_print("Msg Prefix Errors", stat_msg_prefix_errors, 1);
        stat_print("Msg Payload Errors", stat_msg_payload_errors, 1);
        stat_print("Msg Checksum Errors", stat_msg_checksum_errors, 1);
#endif //LOOKER_COMBO
    }

    webString += "    </body>\n";
    webString += "</html>\n";

    server.send(200, "text/html", webString);
}

void ticker_handler()
{
#ifndef LOOKER_COMBO
    if (msg_timeout)
        if (!--msg_timeout)
        {
            MSG_NEW;
            stat_msg_timeouts++;
        }
//todo: remove ms_timeout from combo
    if (ms_timeout > MS_TIMEOUT_MIN)
        if (--ms_timeout == 0)
        {
            if (!looker_data_available())
                stat_ms_timeouts++;
        }
#endif //LOOKER_COMBO

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
//todo: copy args to buffer - will help with multiple http requests
        form_submitted = 1;
    }
    http_request = 1;
}
 
looker_exit_t master_var_set(size_t i)
{
#ifndef LOOKER_COMBO
    unsigned char ack;
    looker_exit_t err;

    msg_begin(&msg, RESPONSE_VALUE);
    msg.payload[msg.payload_size++] = i;
    memcpy(&msg.payload[msg.payload_size], var[i].value_current, var[i].size);
    msg.payload_size += var[i].size;
    do
    {
        msg_send(&msg);
        if ((err = ack_wait(&ack)) != LOOKER_EXIT_SUCCESS)
        {
            return err;
        }
    } while (ack != ACK_SUCCESS);
#endif //LOOKER_COMBO
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
            memcpy(&value_old, var[i].value_current, var[i].size);

            if (value_new != value_old)
            {
                memcpy(var[i].value_current, &value_new, var[i].size);
                master_var_set(i);
                return;
            }
        break;
        }
        case LOOKER_TYPE_UINT:
        {
            unsigned long long value_new, value_old;
            value_new = atoull(var_value);
            value_old = 0;
            memcpy(&value_old, var[i].value_current, var[i].size);

            if (value_new != value_old)
            {
                memcpy(var[i].value_current, &value_new, var[i].size);
                master_var_set(i);

                if (!strcmp(var[i].name, "looker_debug"))
                {
                    if (!value_new)     //reset stats if looker_debug changes from 1 -> 0
                    {
                        stat_s_loops = 0;
                        stat_s_loops_old = 0;
                        stat_s_loops_s = 0;
                        stat_ms_updates = 0;
                        stat_sm_updates = 0;
                        stat_s_resets = 0;
                        stat_ms_timeouts = 0;
#ifndef LOOKER_COMBO
                        stat_ack_get_failures = 0;
                        stat_ack_send_failures = 0;
                        stat_msg_timeouts = 0;
                        stat_msg_prefix_errors = 0;
                        stat_msg_payload_errors = 0;
                        stat_msg_checksum_errors = 0;
#endif //LOOKER_COMBO
                    }
                }
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
                memcpy(&f, var[i].value_current, var[i].size);
                value_old = (double) f;
            }
            else
                memcpy(&value_old, var[i].value_current, var[i].size);

            if (value_new != value_old)
            {
                if (var[i].size == sizeof(float))
                {
                    f = (float) value_new;
                    memcpy(var[i].value_current, &f, var[i].size);
                }
                else
                    memcpy(var[i].value_current, &value_new, var[i].size);

                master_var_set(i);
                return;
            }
        break;
        }
        case LOOKER_TYPE_STRING:
            if (strcmp((char *) var[i].value_current, var_value) != 0)
            {
                strcpy((char *) var[i].value_current, var_value);
                var[i].size = strlen((char *) var[i].value_current) + 1;
                master_var_set(i);
                return;
            }
        break;
        default:
            return;
    }
    return;
}

#ifndef LOOKER_COMBO
static looker_exit_t register_process(void)
{
    size_t i = 1;

    //only new variable
    if (msg.payload[i++] != var_cnt)
        return LOOKER_EXIT_BAD_PARAMETER;

#ifdef LOOKER_SLAVE_SANITY_TEST
    if (var_cnt >= LOOKER_SLAVE_VAR_COUNT)
        return LOOKER_EXIT_NO_MEMORY;
#endif //LOOKER_SLAVE_SANITY_TEST

#ifdef LOOKER_SLAVE_USE_MALLOC
    var_t *p;
    if ((p = (var_t *) realloc(var, (size_t) ((var_cnt + 1) * sizeof(var_t)))) == NULL)
    {
        //looker_destroy() will free old memory block pointed by var
        return LOOKER_EXIT_NO_MEMORY;
    }
    var = p;
    //because memory block might have moved
    pointers_update();
#endif //LOOKER_SLAVE_USE_MALLOC

    var[var_cnt].size = msg.payload[i++];
    var[var_cnt].type = msg.payload[i++];

    //reset value
    //value will be transferred during update (unless it is zero) not register
    if (var[var_cnt].type == LOOKER_TYPE_STRING)
        var[var_cnt].value_current[0] = 0;
    else
        memset(var[var_cnt].value_current, 0, var[var_cnt].size);

    strcpy(var[var_cnt].name, (char *) &msg.payload[i]);
    i += strlen(var[var_cnt].name) + 1;

    var[var_cnt].label = (looker_label_t) msg.payload[i++];

    //reset style
    var[var_cnt].style_current[0] = 0;

    //set up ssid, pass, domain and looker_debug pointers
    if (var[var_cnt].label == LOOKER_LABEL_SSID)
        network_ssid = (char *) &var[var_cnt].name;
    else if (var[var_cnt].label == LOOKER_LABEL_PASS)
        network_pass = (char *) &var[var_cnt].name;
    if (var[var_cnt].label == LOOKER_LABEL_DOMAIN)
        network_domain = (char *) &var[var_cnt].name;
    if (!strcmp(var[var_cnt].name, "looker_debug"))
        looker_debug_show = (unsigned char *) &var[var_cnt].value_current;

    var_cnt++;
}

static looker_exit_t payload_process(void)
{
    size_t i, j;
    unsigned char ack;
    looker_exit_t err;

    switch (msg.payload[0]) {
        case COMMAND_RESET:
            rebooted = 0;
            var_cnt = 0;
#ifdef LOOKER_SLAVE_USE_MALLOC
            if (var)
            {
                free(var);
                var = NULL;
            }
#endif //LOOKER_SLAVE_USE_MALLOC
            looker_debug_show = NULL;

            if (WiFi.status() == WL_CONNECTED)
                slave_state = LOOKER_SLAVE_STATE_CONNECTED;
            else
                slave_state = LOOKER_SLAVE_STATE_DISCONNECTED;
            stat_s_resets++;
            ack_send(&msg, ACK_SUCCESS);
        break;

        case COMMAND_CONNECT:
            if (slave_state != LOOKER_SLAVE_STATE_CONNECTED)
            {
                network_connect(network_ssid, network_pass);
                led_period(80);
                slave_state = LOOKER_SLAVE_STATE_CONNECTING;
            }
            ack_send(&msg, ACK_SUCCESS);
        break;

        case COMMAND_DISCONNECT:
            WiFi.disconnect();
            ack_send(&msg, ACK_SUCCESS);
        break;
        
        case COMMAND_STATUS_GET:
            ack_send(&msg, ACK_SUCCESS);
            //send status
            msg_begin(&msg, RESPONSE_STATUS);
            msg.payload[msg.payload_size++] = rebooted;
            msg.payload[msg.payload_size++] = slave_state;
            do
            {
                msg_send(&msg);
                if ((err = ack_wait(&ack)) != LOOKER_EXIT_SUCCESS)
                {
                    return err;
                }
            } while (ack != ACK_SUCCESS);
        break;

        case COMMAND_VAR_REG:
            register_process();
            stat_ms_updates++;
            ack_send(&msg, ACK_SUCCESS);
        break;

        case COMMAND_VALUE_SET:
            memcpy(var[msg.payload[1]].value_current, &msg.payload[2], var[msg.payload[1]].size);
            stat_ms_updates++;
            ack_send(&msg, ACK_SUCCESS);
        break;

        case COMMAND_STYLE_SET:
            strcpy(var[msg.payload[1]].style_current, (char *) &msg.payload[2]);
            stat_ms_updates++;
            ack_send(&msg, ACK_SUCCESS);
        break;

        case COMMAND_VALUE_GET:
            ack_send(&msg, ACK_SUCCESS);

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

            //done with values
            msg_begin(&msg, RESPONSE_VALUE_LAST);
            do
            {
                msg_send(&msg);
                if ((err = ack_wait(&ack)) != LOOKER_EXIT_SUCCESS)
                {
                    return err;
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
#endif //LOOKER_COMBO

void setup(void)
{
    slave_setup();
#ifdef LOOKER_COMBO
    master_setup();
#endif //LOOKER_COMBO
}    

void slave_setup(void)
{
#ifndef LOOKER_COMBO
    rebooted = 1;
    serial_init();
#endif //LOOKER_COMBO

    WiFi.disconnect();
    WiFi.mode(WIFI_STA);

    ticker.attach_ms(1, ticker_handler);

    pinMode(ledPin, OUTPUT);
    LED_ON;
    led_period(0);

    PRINT("Slave setup: done\n");
}

void loop(void)
{
    slave_loop();
#ifdef LOOKER_COMBO
    master_loop();
#endif //LOOKER_COMBO
}

void slave_loop(void)
{
    //todo: maybe this is slow, maybe global variable is faster
    if (WiFi.status() == WL_CONNECTED)
    {
        if (slave_state == LOOKER_SLAVE_STATE_CONNECTING)
        {
            ip_addr = WiFi.localIP();
            if (network_domain)
                mdns.begin(network_domain, ip_addr);
            server.on("/", handle_root);
            server.begin();
            led_period(500);
            slave_state = LOOKER_SLAVE_STATE_CONNECTED;
        }
        else if (slave_state == LOOKER_SLAVE_STATE_DISCONNECTED)
        {
            led_period(500);
            slave_state = LOOKER_SLAVE_STATE_CONNECTED;
        }
        server.handleClient();

#ifdef LOOKER_COMBO
        if (form_submitted)
        {
            size_t i,j;
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
#endif //LOOKER_COMBO
    }
    else
    {
        if (slave_state != LOOKER_SLAVE_STATE_CONNECTING)
            slave_state = LOOKER_SLAVE_STATE_DISCONNECTED;
    }

#ifndef LOOKER_COMBO
    if (looker_data_available())
    {
        unsigned char err = msg_get(&msg);
        if (!err)
        {
            if (msg_complete(&msg))
            {
#ifdef DEBUG_MSG_DECODE
                msg_decode(&msg, 1);
#endif //DEBUG_MSG_DECODE
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
            ack_send(&msg, ACK_FAILURE);
        }
    }
#else
    switch (network) {
        case LOOKER_NETWORK_DO_NOTHING:
        break;

        case LOOKER_NETWORK_STAY_CONNECTED:
            switch (slave_state) {
                case LOOKER_SLAVE_STATE_CONNECTING:
                    //timeout
                break;
                case LOOKER_SLAVE_STATE_CONNECTED:
                    //task complete
                break;
                case LOOKER_SLAVE_STATE_DISCONNECTING:
                case LOOKER_SLAVE_STATE_DISCONNECTED:
                    if (slave_state != LOOKER_SLAVE_STATE_CONNECTED)
                    {
                        //connect to the network
                        network_connect(network_ssid, network_pass);
                        led_period(80);
                        slave_state = LOOKER_SLAVE_STATE_CONNECTING;
                    }
                break;
                default:
                break;
            }
        break;

        case LOOKER_NETWORK_STAY_DISCONNECTED:
            switch (slave_state) {
                case LOOKER_SLAVE_STATE_CONNECTING:
                case LOOKER_SLAVE_STATE_CONNECTED:
                    //disconnect from the network
                    WiFi.disconnect();
                break;
                case LOOKER_SLAVE_STATE_DISCONNECTING:
                    //timeout
                break;
                case LOOKER_SLAVE_STATE_DISCONNECTED:
                    //task complete
                break;
                default:
                break;
            }
        break;

        case LOOKER_NETWORK_RECONNECT:
            switch (slave_state) {
                case LOOKER_SLAVE_STATE_CONNECTING:
                case LOOKER_SLAVE_STATE_CONNECTED:
                    //disconnect from the network
                    WiFi.disconnect();
                break;
                case LOOKER_SLAVE_STATE_DISCONNECTING:
                    //timeout
                break;
                case LOOKER_SLAVE_STATE_DISCONNECTED:
                    network = LOOKER_NETWORK_RECONNECT_2;
                break;
                default:
                break;
            }
        break;

        case LOOKER_NETWORK_RECONNECT_2:
            switch (slave_state) {
                case LOOKER_SLAVE_STATE_CONNECTING:
                    //timeout
                break;
                case LOOKER_SLAVE_STATE_CONNECTED:
                    network = LOOKER_NETWORK_STAY_CONNECTED;
                break;
                case LOOKER_SLAVE_STATE_DISCONNECTING:
                    network = LOOKER_NETWORK_RECONNECT;
                break;
                case LOOKER_SLAVE_STATE_DISCONNECTED:
                    if (slave_state != LOOKER_SLAVE_STATE_CONNECTED)
                    {
                        //connect to the network
                        network_connect(network_ssid, network_pass);
                        led_period(80);
                        slave_state = LOOKER_SLAVE_STATE_CONNECTING;
                    }
                break;
                default:
                break;
            }
        break;
        default:
        break;
    }

    if (http_request)
    {
        print_html();
        http_request = 0;
    }        

    stat_s_loops++;
#endif //LOOKER_COMBO

    if ((ms_timeout < 0) && (http_request))
    {
        print_html();
        form_submitted = 0;
        http_request = 0;
    }
}

