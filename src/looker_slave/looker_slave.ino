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

#include <errno.h>
#include <float.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
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
//todo: add LOOKER_ to var names because of the combo mode

//globals
looker_slave_state_t slave_state = LOOKER_SLAVE_STATE_DISCONNECTED;
#ifdef LOOKER_SLAVE_USE_MALLOC
    static var_t *var = NULL;
#else
    static var_t var[LOOKER_SLAVE_VAR_COUNT];
#endif //LOOKER_SLAVE_USE_MALLOC
long sid = 0;   //sesion id
size_t slave_var_cnt;    //number of variables in slave's db
unsigned char http_request;
int server_arg = -1;
unsigned char debug;
unsigned char json;

#ifndef LOOKER_COMBO
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
looker_slave_state_task_t slave_state_task = LOOKER_SLAVE_STATE_TASK_DO_NOTHING;

int ledPin = 2; // GPIO2 on ESP8266
size_t led_cnt, led_timeout;
ESP8266WebServer server(80);
MDNSResponder mdns;
IPAddress ip_addr;
String webString="";
Ticker ticker;
int mdns_enabled = 0;

#define TIMEOUT_CNT 100

//prototypes
static void slave_setup(void);
static void slave_loop(void);
void print_u64(char *v, unsigned long long i);
void print_i64(char *v, long long i);
void URL_response_print(void);
void json_print(void);
void html_print(void);
static looker_exit_t network_connect(char *ssid, char *pass);
static void sid_new(long *sid);
static void pointers_update(void);
void stat_print(String name, size_t value, int critical);
void debug_reset();
#ifndef LOOKER_COMBO
    static looker_exit_t payload_process(msg_t *msg);
#endif //LOOKER_COMBO

void debug_reset()
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

//sets new session id to relaod the page
static void sid_new(long *sid)
{
    if (*sid == 0) {
        randomSeed(ESP.getCycleCount());
        *sid = random(1, 0xFFFF);
    } else {
        (*sid)++;
        if (*sid > 0xFFFF) *sid = 1;
    }
}

//todo: also static or enough if only in prototype
void pointers_update(void)
{
    size_t i;
    for (i=0; i<slave_var_cnt; i++)
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
    }
}

looker_exit_t network_connect(const char *ssid, const char *pass)
{
    if (!ssid)
        return LOOKER_EXIT_WRONG_PARAMETER;

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
    slave_var_cnt = 0;
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

looker_exit_t looker_wifi_connect(const char *ssid, const char *pass, const char *domain)
{
    looker_destroy();
    looker_init();

    network_ssid = ssid;
    network_pass = pass;
    network_domain = domain;
    slave_state_task = LOOKER_SLAVE_STATE_TASK_RECONNECT;

    return LOOKER_EXIT_SUCCESS;
}

looker_exit_t looker_reg(const char *name, volatile void *addr, int size, looker_type_t type, looker_label_t label, looker_style_t style)
{
    if ((type == LOOKER_TYPE_STRING) && (label != LOOKER_LABEL_SSID) && (label != LOOKER_LABEL_PASS) && (label != LOOKER_LABEL_DOMAIN))
        size = strlen((const char *) addr) + 1;

#ifdef LOOKER_SLAVE_SANITY_TEST
    if (slave_var_cnt >= LOOKER_SLAVE_VAR_COUNT)
        return LOOKER_EXIT_NO_MEMORY;

    if (!name)
        return LOOKER_EXIT_WRONG_PARAMETER;

    if (strlen(name) + 1 > LOOKER_SLAVE_VAR_NAME_SIZE)
        return LOOKER_EXIT_WRONG_PARAMETER;

    if (size > LOOKER_SLAVE_VAR_VALUE_SIZE)
        return LOOKER_EXIT_WRONG_PARAMETER;

    if (type >= LOOKER_TYPE_LAST)
        return LOOKER_EXIT_WRONG_PARAMETER;

    if ((type >= LOOKER_TYPE_FLOAT_0) && (type <= LOOKER_TYPE_FLOAT_4) && (size != sizeof(float)) && (size != sizeof(double)))
        return LOOKER_EXIT_WRONG_PARAMETER;

    if (label >= LOOKER_LABEL_LAST)
        return LOOKER_EXIT_WRONG_PARAMETER;

#if LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE
    if (style && *style &&(strlen(*style) + 1 > LOOKER_VAR_STYLE_SIZE))
#else
    if (style &&(strlen(style) + 1 > LOOKER_VAR_STYLE_SIZE))
#endif //LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE
        return LOOKER_EXIT_WRONG_PARAMETER;

    size_t i;
    //one ssid, one pass, one domain
    if ((label == LOOKER_LABEL_SSID) || (label == LOOKER_LABEL_PASS) || (label == LOOKER_LABEL_DOMAIN))
    {
        //in combo mode ssid, pass and domain are already set up in connect function
        return LOOKER_EXIT_WRONG_PARAMETER;
    }
    else
    {
        //name cannot be duplicated
        for (i=0; i<slave_var_cnt; i++)
            if ((var[i].name) && (!strcmp(var[i].name, name)))
                return LOOKER_EXIT_WRONG_PARAMETER;
    }
#endif //LOOKER_SLAVE_SANITY_TEST

#ifdef LOOKER_SLAVE_USE_MALLOC
    var_t *p;
    if ((p = (var_t *) realloc(var, (size_t) ((slave_var_cnt + 1) * sizeof(var_t)))) == NULL)
    {
        //looker_destroy() will free old memory block pointed by var
        return LOOKER_EXIT_NO_MEMORY;
    }
    var = p;
    //because memory block might have moved
    pointers_update();
#endif //LOOKER_SLAVE_USE_MALLOC

    var[slave_var_cnt].name = name;
    var[slave_var_cnt].value_current = (void *) addr;
    if (type == LOOKER_TYPE_STRING)
        var[slave_var_cnt].size = 0;
    else
        var[slave_var_cnt].size = size;
    var[slave_var_cnt].type = type;
    var[slave_var_cnt].label = label;
#if ((LOOKER_SLAVE_STYLE == LOOKER_STYLE_FIXED) || (LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE))
    var[slave_var_cnt].style_current = style;
#endif //((LOOKER_SLAVE_STYLE == LOOKER_STYLE_FIXED) || (LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE))

    //set new session id to relaod the page
    sid_new(&sid);

    slave_var_cnt++;
    return LOOKER_EXIT_SUCCESS;
}

looker_exit_t looker_update(void)
{
    ms_timeout = MS_TIMEOUT;
    return LOOKER_EXIT_SUCCESS;
}
#endif //LOOKER_COMBO

void stat_print(String name, size_t value, int critical)
{
    if (name) {
        webString += "        <p id='";
        webString += name;
        if (value && critical)
            webString += "' style='margin:0;color:red;'>";
        else
            webString += "' style='margin:0;color:black;'>";
        webString += name + ": " + String(value);
        webString += "</p>\n";
    }
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

void URL_response_print(void)
{
    if (json)
        json_print();
    else
        html_print();
}

void json_print(void)
{
    StaticJsonDocument<512> jsonDoc;
    char value_text[(LOOKER_SLAVE_VAR_VALUE_SIZE > 20) ? (LOOKER_SLAVE_VAR_VALUE_SIZE + 1) : (20 + 1)]; //20 is the length of max 64-bit number
    size_t i;

    server.sendHeader("Access-Control-Allow-Origin", "*");

    if (ms_timeout >= 0) {
        jsonDoc["looker_sid"] = String(sid);
        for (i=0; i<slave_var_cnt; i++)
        {
            //skip some vars
            if ((var[i].label == LOOKER_LABEL_SSID) || (var[i].label == LOOKER_LABEL_PASS) || (var[i].label == LOOKER_LABEL_DOMAIN))
                continue;

            if (var[i].type == LOOKER_TYPE_INT)
            {
                switch (var[i].size) {
                    case 1:
                        sprintf(value_text, "%d", *(int8_t *) var[i].value_current);
                        break;
                    case 2:
                        sprintf(value_text, "%d", *(int16_t *) var[i].value_current);
                        break;
                    case 4:
                        sprintf(value_text, "%d", *(int32_t *) var[i].value_current);
                        break;
                    case 8:
                        print_i64(value_text, *(int64_t *) var[i].value_current);
                        break;
                    }
            }
            else if (var[i].type == LOOKER_TYPE_UINT)
            {
                unsigned long long value_current = 0ULL;
                memcpy(&value_current, var[i].value_current, var[i].size);
                if (var[i].size == 8)
                    print_u64(value_text, value_current);
                else
                    sprintf(value_text, "%u", value_current);
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

            jsonDoc[var[i].name] = value_text;
        }

        if (debug) {
            jsonDoc["looker_S_State"] = String(slave_state);
            jsonDoc["looker_S_Loops"] = String(stat_s_loops);
            jsonDoc["looker_S_Loops/s"] = String(stat_s_loops_s);
            jsonDoc["looker_S_Resets"] = String(stat_s_resets);
            jsonDoc["looker_MS_Updates"] = String(stat_ms_updates);
            jsonDoc["looker_MS_Timeouts"] = String(stat_ms_timeouts);
#ifndef LOOKER_COMBO
            jsonDoc["looker_MS_ACK_Failures"] = String(stat_ack_get_failures);
//todo: to be implemented
//            jsonDoc["looker_MS_Msg_Timeouts"] = String(stat_msg_timeouts);
            jsonDoc["looker_MS_Msg_Prefix_Errors"] = String(stat_msg_prefix_errors);
            jsonDoc["looker_MS_Msg_Payload_Errors"] = String(stat_msg_payload_errors);
            jsonDoc["looker_MS_Msg_Checksum_Errors"] = String(stat_msg_checksum_errors);
            jsonDoc["looker_SM_ACK_Failures"] = String(stat_ack_send_failures);
#endif //LOOKER_COMBO
            jsonDoc["looker_SM_Updates"] = String(stat_sm_updates);
        }
    } else
        jsonDoc["looker_timeout"] = String((MS_TIMEOUT-ms_timeout)/1000);

    webString = "";
    serializeJson(jsonDoc, webString);
    server.send(200, "text/json", webString);
}

void html_print(void)
{
    server.sendHeader("Access-Control-Allow-Origin", "*");
    webString =
    "<!doctype html>\n"
    "<html>\n"
    "<head>\n"
    "    <title>";
    if (network_domain)
        webString += String(network_domain);
    else
        webString += "Looker";
    webString +=
    "</title>\n"
    "</head>\n"
    "<body>\n";

    if (network_domain)
        webString += "    <h2>" + String(network_domain) + "</h2>\n";

    webString +=
    "    <table><tr>\n"
    "    <td><button id='refresh_id' onclick='refresh_func()'></button></td>\n"
    "    <td id='looker_updated' style='color:white;'>&#8226;</td>\n"
    "    <td id='looker_timeout'></td>\n"
    "    </tr></table>\n"
    "    <br>\n";

    webString += "    <form method='post'>\n";

    for (int i=0; i<slave_var_cnt; i++)
    {
        char value_text[(LOOKER_SLAVE_VAR_VALUE_SIZE > 20) ? (LOOKER_SLAVE_VAR_VALUE_SIZE + 1) : (20 + 1)]; //20 is the length of max 64-bit number

        if ((var[i].label == LOOKER_LABEL_SSID) || (var[i].label == LOOKER_LABEL_PASS) || (var[i].label == LOOKER_LABEL_DOMAIN))
            continue;

        if (var[i].type == LOOKER_TYPE_INT)
        {
            switch (var[i].size) {
                case 1:
                    sprintf(value_text, "%d", *(int8_t *) var[i].value_current);
                    break;
                case 2:
                    sprintf(value_text, "%d", *(int16_t *) var[i].value_current);
                    break;
                case 4:
                    sprintf(value_text, "%d", *(int32_t *) var[i].value_current);
                    break;
                case 8:
                    print_i64(value_text, *(int64_t *) var[i].value_current);
                    break;
                }
        }
        else if (var[i].type == LOOKER_TYPE_UINT)
        {
            unsigned long long value_current = 0ULL;
            memcpy(&value_current, var[i].value_current, var[i].size);
            if (var[i].size == 8)
                print_u64(value_text, value_current);
            else
                sprintf(value_text, "%u", value_current);
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

        webString += "        ";
        if (var[i].label == LOOKER_LABEL_VIEW) {
            webString += "<p id='";
            webString += var[i].name;
            webString += "'";
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
                webString += " style='";
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
                webString += ">";

            webString += var[i].name;
            webString += ": ";
            webString += value_text;
        }
        else
        {
            webString += "<p>";
            webString += var[i].name;
            webString += ": ";
        }

        if (var[i].label == LOOKER_LABEL_EDIT)
        {
            webString += "<input id='";
            webString += var[i].name;
            webString += "' ";
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
            webString += "        ";
            webString += "<input id='";
            webString += var[i].name;
            webString += "' ";
            webString += "type='checkbox' name='";
            webString += var[i].name;
            webString += "' value='1'";
            if (strcmp(value_text, "0"))
                webString += " checked";
            webString += ">";
        }
        else if (var[i].label == LOOKER_LABEL_CHECKBOX_INV)
        {
            webString += "<input type='hidden' name='";
            webString += var[i].name;
            webString += "' value='1'>\n";

            webString += "        ";
            webString += "<input id='";
            webString += var[i].name;
            webString += "' ";
            webString += "type='checkbox' name='";
            webString += var[i].name;
            webString += "' value='0'";
            if (!strcmp(value_text, "0"))
                webString += " checked";
            webString += ">";
        }
        webString += "</p>\n";
    }

    if (!slave_var_cnt)
        webString += "No Variables<br>\n";
    else {
        webString += "        <br>\n";
        webString += "        <input type='submit' value='Submit'><br>\n";
    }
    webString += "    </form>\n";

    if (debug)
    {
        webString += "        <h2>Debug:</h2>\n";
        stat_print("looker_S_State", slave_state, 0);
        stat_print("looker_S_Loops", stat_s_loops, 0);
        stat_print("looker_S_Loops/s", stat_s_loops_s, 0);
        stat_print("looker_S_Resets", stat_s_resets, 0);
        stat_print("looker_MS_Updates", stat_ms_updates, 0);
        stat_print("looker_MS_Timeouts", stat_ms_timeouts, 1);
#ifndef LOOKER_COMBO
        stat_print("looker_MS_ACK_Failures", stat_ack_get_failures, 1);
//todo: to be implemented
//        stat_print("looker_MS_Msg_Timeouts", stat_msg_timeouts, 1);
        stat_print("looker_MS_Msg_Prefix_Errors", stat_msg_prefix_errors, 1);
        stat_print("looker_MS_Msg_Payload_Errors", stat_msg_payload_errors, 1);
        stat_print("looker_MS_Msg_Checksum_Errors", stat_msg_checksum_errors, 1);
        stat_print("looker_SM_ACK_Failures", stat_ack_send_failures, 1);
#endif //LOOKER_COMBO
        stat_print("looker_SM_Updates", stat_sm_updates, 0);
    }

    webString +=
    "    <script>\n"
    "    var refresh_label = 'Turn refresh OFF';\n"
    "    //var refresh_label = 'Turn refresh ON';\n"
    "    var refresh_timeout;\n"
    "    document.getElementById('refresh_id').innerHTML = refresh_label;\n";
    if (debug)
        webString += "    get(window.location.href.split('?')[0] + '?looker_debug=1&looker_json=1');\n";
    else
        webString += "    get(window.location.href.split('?')[0] + '?looker_json=1');\n";
    webString +=
    "\n"
    "    function submit() {\n"
    "        document.getElementById('form').submit();\n"
    "    }\n"
    "\n"
    "    function onLoad () {\n"
    "        var json_obj = this.response;\n"
    "        var timeout = 0;\n"
    "        for (var key in json_obj) {\n"
    "            if (key === 'looker_sid') {\n"
    "                if (json_obj['looker_sid'] !== ";
    webString += "'" + String(sid) + "')\n"
    "                    window.location.reload();\n"
    "            }\n"
    "            if (key === 'looker_timeout') {\n"
    "                timeout = 1;\n"
    "                continue;\n"
    "            }\n"
    "            var obj = document.getElementById(key);\n"
    "            if (obj === null)\n"
    "                continue;\n"
    "            if (obj.type === 'text')\n"
    "                obj.value = json_obj[key];\n"
    "            else if (obj.type === 'checkbox') {\n"
    "                if (json_obj[key] === '1')\n"
    "                    obj.checked = true;\n"
    "                else\n"
    "                    obj.checked = false;\n"
    "            }\n"
    "            else\n"
    "                obj.innerHTML = key + ': ' + json_obj[key];\n"
    "        }\n"
    "        if ((timeout === 1) && (refresh_label == 'Turn refresh OFF'))\n"
    "            document.getElementById('looker_timeout').innerHTML = 'Master timeout: ' + json_obj['looker_timeout'];\n"
    "        else\n"
    "            document.getElementById('looker_timeout').innerHTML = '';\n"
    "        if (refresh_label == 'Turn refresh OFF')\n";
    if (debug)
        webString += "            refresh_timeout = window.setTimeout(get,1000,window.location.href.split('?')[0] + '?looker_debug=1&looker_json=1');\n";
    else
        webString += "            refresh_timeout = window.setTimeout(get,1000,window.location.href.split('?')[0] + '?looker_json=1');\n";
    webString +=
    "        document.getElementById('looker_updated').style='color:black;';\n"
    "        updated_timeout = window.setTimeout(function(){document.getElementById('looker_updated').style='color:white;';}, 200, null);\n"
    "    }\n"
    "\n"
    "    function onError () {\n"
    "        clearTimeout(refresh_timeout);\n"
    "        document.getElementById('looker_timeout').innerHTML = 'Server timeout, reconnecting ...';\n"
    "        document.getElementById('refresh_id').innerHTML = refresh_label;\n";
    if (debug)
        webString += "        get(window.location.href.split('?')[0] + '?looker_debug=1&looker_json=1');\n";
    else
        webString += "        get(window.location.href.split('?')[0] + '?looker_json=1');\n";
    webString +=
    "    }\n"
    "\n"
    "    function get(url) {\n"
    "        var xhr = new XMLHttpRequest();\n"
    "        xhr.addEventListener('load', onLoad);\n"
    "        xhr.addEventListener('error', onError);\n"
    "        xhr.responseType = 'json';\n"
    "        xhr.open('GET',url);\n"
    "        xhr.send();\n"
    "    }\n"
    "\n"
    "    function refresh_func() {\n"
    "        if (refresh_label == 'Turn refresh OFF') {\n"
    "            clearTimeout(refresh_timeout);\n"
    "            refresh_label = 'Turn refresh ON';\n"
    "            document.getElementById('looker_timeout').innerHTML = '';\n"
    "        }\n"
    "        else {\n"
    "            refresh_label = 'Turn refresh OFF';\n";
    if (debug)
        webString += "            get(window.location.href.split('?')[0] + '?looker_debug=1&looker_json=1');\n";
    else
        webString += "            get(window.location.href.split('?')[0] + '?looker_json=1');\n";
    webString +=
    "        }\n"
    "        document.getElementById('refresh_id').innerHTML = refresh_label;\n"
    "    }\n"
    "    </script>\n";

    webString += "</body>\n";
    webString += "</html>\n";

    server.send(200, "text/html", webString);
}

void ticker_handler()
{
#ifndef LOOKER_COMBO
    if (msg_timeout)
        if (!--msg_timeout)
        {
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
    json = 0;
    debug = 0;

    for (size_t i=0; i<server.args(); i++) {
        if (!strcmp(server.argName(i).c_str(), "looker_json"))
//todo: check also if value = 1
            json = 1;
        else if (!strcmp(server.argName(i).c_str(), "looker_debug")) {
            if (!strcmp(server.arg(i).c_str(), "0"))
                debug_reset();
            else
                debug = 1;
        }
        if (json && debug)
            break;
    }

    if (server.args())
        server_arg = 0; //0 - index of first arg to process

    http_request = 1;
}
 
looker_exit_t master_var_set(size_t i)
{
#ifndef LOOKER_COMBO
    msg_t msg;
    unsigned char ack;
    looker_exit_t err;

    msg_begin(&msg, RESPONSE_VALUE);
    msg.payload[msg.payload_size++] = i;

    if (var[i].type == LOOKER_TYPE_STRING)
    {
        strcpy((char *) &msg.payload[msg.payload_size], (const char *) var[i].value_current);
        msg.payload_size += (strlen((const char *) var[i].value_current) + 1);
    }
    else
    {
        memcpy(&msg.payload[msg.payload_size], var[i].value_current, var[i].size);
        msg.payload_size += var[i].size;
    }
    msg_send(&msg);
#endif //LOOKER_COMBO
    return LOOKER_EXIT_SUCCESS;
}

//find and replace variable
//web server -> slave -> master
//return:
//0 - could not find
//1 - found and replaced
unsigned char var_replace(const char *var_name, const char *var_value)
{
    if (!slave_var_cnt)
        return 0;

    //find
    int i = 0;
    while ((i < slave_var_cnt) && (strcmp(var[i].name, var_name) != 0))
        i++;

    if (i == slave_var_cnt)
        return 0;

    //replace if different
    switch (var[i].type) {
        case LOOKER_TYPE_INT:
        {
            long long value_new;
            char *endptr;

            errno = 0;
            value_new = strtoll(var_value, &endptr, 10);
            if ((*endptr != 0) || (errno == ERANGE))
                break;

            switch (var[i].size) {
                case 1:
                    if ((value_new < -128LL) || (value_new > 127LL))
                        return 0;
                    if ((int8_t) value_new == *(int8_t *) var[i].value_current)
                        return 0;
                    *(int8_t *) var[i].value_current = (int8_t) value_new;
                    break;
                case 2:
                    if ((value_new < -32768LL) || (value_new > 32767LL))
                        return 0;
                    if ((int16_t) value_new == *(int16_t *) var[i].value_current)
                        return 0;
                    *(int16_t *) var[i].value_current = (int16_t) value_new;
                    break;
                case 4:
                    if ((value_new < -2147483648LL) || (value_new > 2147483647LL))
                        return 0;
                    if ((int32_t) value_new == *(int32_t *) var[i].value_current)
                        return 0;
                    *(int32_t *) var[i].value_current = (int32_t) value_new;
                    break;
                case 8:
                    //range was alredy checked
                    if (value_new == *(int64_t *) var[i].value_current)
                        return 0;
                    *(int64_t *) var[i].value_current = value_new;
                    break;
                default:
                    return 0;
            }
            master_var_set(i);
            return 1;
        }
        case LOOKER_TYPE_UINT:
        {
            unsigned long long value_new;
            unsigned long long value_current = 0ULL;
            char *endptr;

            errno = 0;
            value_new = strtoull(var_value, &endptr, 10);
            if ((*endptr != 0) || (errno == ERANGE))
                break;

            switch (var[i].size) {
                case 1:
                    if (value_new > 255ULL)
                        return 0;
                    break;
                case 2:
                    if (value_new > 65535ULL)
                        return 0;
                    break;
                case 4:
                    if (value_new > 4294967295ULL)
                        return 0;
                    break;
                case 8:
                    //range was alredy checked
                    break;
                default:
                    return 0;
            }

            memcpy(&value_current, var[i].value_current, var[i].size);
            if (value_current != value_new)
            {
                memcpy(var[i].value_current, &value_new, var[i].size);
                master_var_set(i);
                return 1;
            }
            break;
        }
        case LOOKER_TYPE_FLOAT_0:
        case LOOKER_TYPE_FLOAT_1:
        case LOOKER_TYPE_FLOAT_2:
        case LOOKER_TYPE_FLOAT_3:
        case LOOKER_TYPE_FLOAT_4:
        {
            double value_new;
            char *endptr;
            char err;

            errno = 0;
            value_new = strtod(var_value, &endptr);
            if ((*endptr != 0) || (errno == ERANGE))
                break;

            switch (var[i].size) {
                case 4:
                    if (((value_new < -FLT_MAX) || (value_new > FLT_MAX))
                      || ((value_new > -FLT_MIN) && (value_new < FLT_MIN)))
                        return 0;
                    if (*(float *) var[i].value_current == (float ) value_new)
                        return 0;
                    *(float *) var[i].value_current = (float) value_new;
                    break;
                case 8:
                    //range was alredy checked
                    if (*(double *) var[i].value_current == value_new)
                        return 0;
                    *(double *) var[i].value_current = value_new;
                    break;
                default:
                    return 0;
            }
            master_var_set(i);
            return 1;
        }
        case LOOKER_TYPE_STRING:
            if (strcmp((char *) var[i].value_current, var_value) != 0)
            {
#ifdef LOOKER_SLAVE_SANITY_TEST
//todo: LOOKER_SLAVE_VAR_VALUE_SIZE might be still > than LOOKER_MASTER_VAR_VALUE_SIZE
                if ((strlen(var_value) + 1) <= LOOKER_SLAVE_VAR_VALUE_SIZE)
#endif //LOOKER_SLAVE_SANITY_TEST
                {
                    strcpy((char *) var[i].value_current, var_value);
                    master_var_set(i);
                    return 1;
                }
            }
            break;
        default:
            return 0;
    }
    return 0;
}

#ifndef LOOKER_COMBO
static looker_exit_t register_process(msg_t *msg)
{
    size_t i = 1;

    //only new variable
    if (msg->payload[i++] != slave_var_cnt)
        return LOOKER_EXIT_WRONG_PARAMETER;

#ifdef LOOKER_SLAVE_SANITY_TEST
    if (slave_var_cnt >= LOOKER_SLAVE_VAR_COUNT)
        return LOOKER_EXIT_NO_MEMORY;
#endif //LOOKER_SLAVE_SANITY_TEST

#ifdef LOOKER_SLAVE_USE_MALLOC
    var_t *p;
    if ((p = (var_t *) realloc(var, (size_t) ((slave_var_cnt + 1) * sizeof(var_t)))) == NULL)
    {
        //looker_destroy() will free old memory block pointed by var
        return LOOKER_EXIT_NO_MEMORY;
    }
    var = p;
    //because memory block might have moved
    pointers_update();
#endif //LOOKER_SLAVE_USE_MALLOC

    var[slave_var_cnt].size = msg->payload[i++];
    var[slave_var_cnt].type = msg->payload[i++];

    //reset value
    //value will be transferred (unless it is zero) during update not register
    if (var[slave_var_cnt].type == LOOKER_TYPE_STRING)
        var[slave_var_cnt].value_current[0] = 0;
    else
        memset(var[slave_var_cnt].value_current, 0, var[slave_var_cnt].size);

    strcpy(var[slave_var_cnt].name, (char *) &msg->payload[i]);
    i += strlen(var[slave_var_cnt].name) + 1;

    var[slave_var_cnt].label = (looker_label_t) msg->payload[i++];

    //reset style
    var[slave_var_cnt].style_current[0] = 0;

    //set up ssid, pass and domain
    if (var[slave_var_cnt].label == LOOKER_LABEL_SSID)
        network_ssid = (char *) &var[slave_var_cnt].name;
    else if (var[slave_var_cnt].label == LOOKER_LABEL_PASS)
        network_pass = (char *) &var[slave_var_cnt].name;
    if (var[slave_var_cnt].label == LOOKER_LABEL_DOMAIN)
        network_domain = (char *) &var[slave_var_cnt].name;

    slave_var_cnt++;
}

//master -> slave
static looker_exit_t payload_process(msg_t *msg)
{
    size_t i, j;
    unsigned char ack;
    looker_exit_t err;

    switch (msg->payload[0]) {
        case COMMAND_RESET:
            //re-connection takes time so reset does not disrupt connection
            rebooted = 0;
            slave_var_cnt = 0;
            //set new session id to relaod the page
            sid_new(&sid);
#ifdef LOOKER_SLAVE_USE_MALLOC
            if (var)
            {
                free(var);
                var = NULL;
            }
#endif //LOOKER_SLAVE_USE_MALLOC

            server_arg = -1;
            stat_s_resets++;
            ack_send(RESPONSE_ACK_SUCCESS);
        break;

        case COMMAND_CONNECT:
            if (slave_state != LOOKER_SLAVE_STATE_CONNECTED)
            {
                network_connect(network_ssid, network_pass);
                led_period(80);
                slave_state = LOOKER_SLAVE_STATE_CONNECTING;
            }
            ack_send(RESPONSE_ACK_SUCCESS);
        break;

        case COMMAND_DISCONNECT:
            WiFi.disconnect();
            slave_state = LOOKER_SLAVE_STATE_DISCONNECTING;
            ack_send(RESPONSE_ACK_SUCCESS);
        break;
        
        case COMMAND_STATUS_GET:
            //send status
            {
                msg_t msg;
                msg_begin(&msg, RESPONSE_STATUS);
                msg.payload[msg.payload_size++] = rebooted;
                msg.payload[msg.payload_size++] = slave_state;
                msg_send(&msg);
            }
        break;

        case COMMAND_VAR_REG:
            register_process(msg);
//todo: report exit code to master
            //set new session id to relaod the page
            sid_new(&sid);
            stat_ms_updates++;
            ack_send(RESPONSE_ACK_SUCCESS);
        break;

        case COMMAND_VALUE_SET:
            if (var[msg->payload[1]].type == LOOKER_TYPE_STRING)
            {
#ifdef LOOKER_SLAVE_SANITY_TEST
                if ((strlen((const char *) &msg->payload[2]) + 1 ) <= LOOKER_SLAVE_VAR_VALUE_SIZE)
#endif //LOOKER_SLAVE_SANITY_TEST
                    strcpy(var[msg->payload[1]].value_current, (const char *) &msg->payload[2]);
            }
            else
                memcpy(var[msg->payload[1]].value_current, &msg->payload[2], var[msg->payload[1]].size);
//todo: report exit code to master
            stat_ms_updates++;
            ack_send(RESPONSE_ACK_SUCCESS);
        break;

        case COMMAND_STYLE_SET:
            strcpy(var[msg->payload[1]].style_current, (char *) &msg->payload[2]);
            stat_ms_updates++;
            ack_send(RESPONSE_ACK_SUCCESS);
        break;

        case COMMAND_VALUE_REPEAT:
            if (server_arg > 0)
                server_arg--;
            //no break on purpose

        case COMMAND_VALUE_GET:
            if (server_arg >= 0)
            {
                do {
                    do {
                        i = (unsigned int) server_arg;
                        //process only last variable with same name
                        j = i+1;
                        while ((j<server.args()) && (strcmp(server.argName(i).c_str(), server.argName(j).c_str()) != 0))
                            j++;
                        if (j < server.args())
                            server_arg++;
                    } while (j < server.args());
                } while ((!var_replace(server.argName(i).c_str(), server.arg(i).c_str())) && (++server_arg < server.args()));
                if (server_arg < server.args())
                {
                    stat_sm_updates++;
                    break;
                }
            }
            //done with values
            msg_t msg;
            msg_begin(&msg, RESPONSE_VALUE_NO_MORE);
            msg_send(&msg);

            //all args were processed
            server_arg = -1;

            //all good
            if (http_request)
            {
                URL_response_print();
                http_request = 0;
            }
                
            stat_s_loops++;
            ms_timeout = MS_TIMEOUT;

        break;

        default:
            return LOOKER_EXIT_WRONG_COMMAND;
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
    looker_stubs_init(NULL);
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
            if (network_domain) {
                if (mdns.begin(network_domain, ip_addr)) {
                    mdns_enabled=1;
                    mdns.addService("http", "tcp", 80);
                } else mdns_enabled=0;
            } else mdns_enabled=0;

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

        //only if no args to process
        if (server_arg < 0)
            server.handleClient();
        if (mdns_enabled)
            mdns.update();

#ifdef LOOKER_COMBO
        size_t i, j;
        if (server_arg >= 0)
        {
            do {
                do {
                    i = (unsigned int) server_arg;
                    //process only last variable with same name
                    j = i+1;
                    while ((j<server.args()) && (strcmp(server.argName(i).c_str(), server.argName(j).c_str()) != 0))
                        j++;
                    if (j < server.args())
                        server_arg++;
                } while (j < server.args());
                if (var_replace(server.argName(i).c_str(), server.arg(i).c_str()))
                    stat_sm_updates++;
            } while (++server_arg < server.args());
            server_arg = -1;
        }
#endif //LOOKER_COMBO
    }
    else
    {
        if (slave_state != LOOKER_SLAVE_STATE_CONNECTING) {
            slave_state = LOOKER_SLAVE_STATE_DISCONNECTED;
            mdns_enabled=0;
        }
    }

#ifndef LOOKER_COMBO
    if (looker_data_available())
    {
        msg_t msg;
        looker_exit_t err = msg_get(&msg);

        switch (err) {
            case LOOKER_EXIT_SUCCESS:
                msg_timeout = 0;
                payload_process(&msg);
            break;

            case LOOKER_EXIT_WRONG_PREFIX:
                stat_msg_prefix_errors++;
            break;

            case LOOKER_EXIT_WRONG_PAYLOAD_SIZE:
                stat_msg_payload_errors++;
            break;

            case LOOKER_EXIT_WRONG_CHECKSUM:
                stat_msg_checksum_errors++;
            break;

            default:
            break;
        }
        if (err != LOOKER_EXIT_SUCCESS)
        {
            //empty rx buffer
            unsigned char rx;
            while (looker_data_available())
                looker_get(&rx, 1);
            ack_send(RESPONSE_ACK_FAILURE);
        }
    }
#else
    switch (slave_state_task) {
        case LOOKER_SLAVE_STATE_TASK_DO_NOTHING:
        break;

        case LOOKER_SLAVE_STATE_TASK_CONNECT:
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

        case LOOKER_SLAVE_STATE_TASK_DISCONNECT:
            switch (slave_state) {
                case LOOKER_SLAVE_STATE_CONNECTING:
                case LOOKER_SLAVE_STATE_CONNECTED:
                    //disconnect from the network
                    WiFi.disconnect();
                    slave_state = LOOKER_SLAVE_STATE_DISCONNECTING;
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

        case LOOKER_SLAVE_STATE_TASK_RECONNECT:
            switch (slave_state) {
                case LOOKER_SLAVE_STATE_CONNECTING:
                case LOOKER_SLAVE_STATE_CONNECTED:
                    //disconnect from the network
                    WiFi.disconnect();
                    slave_state = LOOKER_SLAVE_STATE_DISCONNECTING;
                break;
                case LOOKER_SLAVE_STATE_DISCONNECTING:
                    //timeout
                break;
                case LOOKER_SLAVE_STATE_DISCONNECTED:
                    slave_state_task = LOOKER_SLAVE_STATE_TASK_CONNECT;
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
        URL_response_print();
        http_request = 0;
    }        

    stat_s_loops++;
#endif //LOOKER_COMBO

    if ((ms_timeout < 0) && (http_request))
    {
        URL_response_print();
        server_arg = -1;
        http_request = 0;
    }
}
