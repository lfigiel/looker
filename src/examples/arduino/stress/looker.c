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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "looker.h"
#include "looker_wifi.h"

typedef struct {
    char name[LOOKER_VAR_NAME_SIZE];
    volatile void *addr;
    unsigned char size;
    unsigned char type;
    char value[LOOKER_VAR_VALUE_SIZE];
#ifdef LOOKER_STYLE_DYNAMIC
    char *style_current;
    char style_saved[LOOKER_VAR_STYLE_SIZE];
#endif //LOOKER_STYLE_DYNAMIC
} looker_var_t;

//globals
#ifdef LOOKER_USE_MALLOC
static looker_var_t *looker_var = NULL;
#else
static looker_var_t looker_var[LOOKER_VAR_COUNT];
#endif //LOOKER_USE_MALLOC
static size_t looker_var_count = 0;

void looker_send_comm(LOOKER_COMM comm)
{
    looker_send(&comm, 1);    
}

LOOKER_EXIT_CODE looker_init(const char *ssid, const char *pass, const char *domain)
{
#ifdef LOOKER_SANITY_TEST
    if ((LOOKER_VAR_COUNT > LOOKER_WIFI_VAR_COUNT) ||
        (LOOKER_VAR_VALUE_SIZE > LOOKER_WIFI_VAR_VALUE_SIZE) ||
        (LOOKER_VAR_NAME_SIZE > LOOKER_WIFI_VAR_NAME_SIZE) ||
        (LOOKER_VAR_STYLE_SIZE > LOOKER_WIFI_VAR_STYLE_SIZE))
        return LOOKER_EXIT_NO_MEMORY;
#endif //LOOKER_SANITY_TEST

    looker_send_comm(LOOKER_COMM_RESET);

    looker_send_comm(LOOKER_COMM_CONNECT);

    looker_send((void *) ssid, strlen(ssid) + 1);
    looker_send((void *) pass, strlen(pass) + 1);

    if (domain)
        looker_send((void *) domain, strlen(domain)+1);
    else
    {
        char z = 0;
        looker_send((void *) &z, 1);
    }

    //get response
    LOOKER_EXIT_CODE err;
    while (!looker_get(&err, 1)) {}
    if (err != LOOKER_EXIT_SUCCESS)
        return err;

    return LOOKER_EXIT_SUCCESS;
}

LOOKER_EXIT_CODE looker_reg(const char *name, volatile void *addr, int size, LOOKER_VAR_TYPE type, LOOKER_HTML_TYPE html, STYLE_TYPE style)
{
    if (type == LOOKER_VAR_STRING)
        size = strlen((const char *) addr) + 1;

#ifdef LOOKER_SANITY_TEST
    if (looker_var_count >= LOOKER_VAR_COUNT)
        return LOOKER_EXIT_NO_MEMORY;

    if (type >= LOOKER_VAR_LAST)
        return LOOKER_EXIT_BAD_PARAMETER;

    if (size > LOOKER_VAR_VALUE_SIZE)
        return LOOKER_EXIT_NO_MEMORY;

    if ((type == LOOKER_VAR_FLOAT) && (size != sizeof(float)) && (size != sizeof(double)))
        return LOOKER_EXIT_BAD_PARAMETER;

    if (strlen(name) + 1 > LOOKER_VAR_NAME_SIZE)
        return LOOKER_EXIT_BAD_PARAMETER;

    if (html >= LOOKER_HTML_LAST)
        return LOOKER_EXIT_BAD_PARAMETER;

    if (style && (strlen(style) + 1 > LOOKER_WIFI_VAR_STYLE_SIZE))
        return LOOKER_EXIT_BAD_PARAMETER;
#endif //LOOKER_SANITY_TEST

#ifdef LOOKER_USE_MALLOC
    looker_var_t *p;
    if ((p = (looker_var_t *) realloc(looker_var, (size_t) ((looker_var_count + 1) * sizeof(looker_var_t)))) == NULL)
    {
        //looker_destroy() will free old memory block pointed by looker_var 
        return LOOKER_EXIT_NO_MEMORY;
    }
    looker_var = p;
#endif //LOOKER_USE_MALLOC

    looker_var[looker_var_count].addr = addr;
    looker_var[looker_var_count].size = size;
    looker_var[looker_var_count].type = type;
    memcpy(looker_var[looker_var_count].value, (const void *) addr, size);

#ifdef LOOKER_STYLE_DYNAMIC
    looker_var[looker_var_count].style_current = style;
    if (style)
        strcpy(looker_var[looker_var_count].style_saved, style);
    else
        looker_var[looker_var_count].style_saved[0] = 0;
#endif //LOOKER_STYLE_DYNAMIC

    strcpy(looker_var[looker_var_count].name, name);

    looker_send_comm(LOOKER_COMM_REG);

    looker_send((void *) &size, 1);
    looker_send((void *) &type, 1);
    looker_send((void *) addr, size);
    looker_send((void *) name, strlen(name)+1);
    looker_send((void *) &html, 1);
    if (style)
        looker_send((void *) style, strlen(style)+1);
    else
    {
        char z = 0;
        looker_send((void *) &z, 1);
    }
    looker_var_count++;

    return LOOKER_EXIT_SUCCESS;
}

LOOKER_EXIT_CODE looker_update(void)
{
    for (int i=0; i<looker_var_count; i++)
    {
        if (looker_var[i].type == LOOKER_VAR_STRING)
        {
            looker_var[i].size = strlen((const char *) looker_var[i].addr) + 1;
#ifdef LOOKER_SANITY_TEST
            if (looker_var[i].size > LOOKER_VAR_VALUE_SIZE)
                return LOOKER_EXIT_NO_MEMORY;
#endif //LOOKER_SANITY_TEST
        }

        //update value
        if (memcmp(looker_var[i].value, (const void *) looker_var[i].addr, looker_var[i].size) != 0)
        {
            looker_send_comm(LOOKER_COMM_UPDATE_VALUE);

            looker_send((void *) &i, 1);
            looker_send((void *) looker_var[i].addr, looker_var[i].size);

            memcpy(looker_var[i].value, (const void *) looker_var[i].addr, looker_var[i].size);
        }

        //update style
#ifdef LOOKER_STYLE_DYNAMIC
        if (looker_var[i].style_current)
        {
            if (strcmp(looker_var[i].style_current, looker_var[i].style_saved) != 0)
            {
                looker_send_comm(LOOKER_COMM_UPDATE_STYLE);

                looker_send((void *) &i, 1);
                looker_send((void *) looker_var[i].style_current, strlen(looker_var[i].style_current)+1);

                strcpy(looker_var[i].style_saved, looker_var[i].style_current);
            }
        }
        else
        if (looker_var[i].style_saved[0])
        {
            looker_send_comm(LOOKER_COMM_UPDATE_STYLE);

            looker_send((void *) &i, 1);
            char z = 0;
            looker_send((void *) &z, 1);
            looker_var[i].style_saved[0] = 0;
        }
#endif //LOOKER_STYLE_DYNAMIC
    }

    //end of update, send handshake
    looker_send_comm(LOOKER_COMM_HANDSHAKE);

    //get remote update and handshake
    unsigned char c;
    char hs = 0;
    int var_number;
    unsigned char pos;
    LOOKER_STATE state = LOOKER_STATE_START;

    while (!hs) 
    {
        //get byte
        while (!looker_get(&c, 1)) {}

        switch (state) {
        case LOOKER_STATE_START:

            pos = 0;
            var_number = -1;
            switch (c) {
            case LOOKER_COMM_UPDATE_VALUE:
                state = LOOKER_STATE_UPDATE_VALUE;
                break;
            case LOOKER_COMM_HANDSHAKE:
                hs = 1;
                break;
            default:
                //bad command
                return LOOKER_EXIT_BAD_COMM;
                break;
            }
            break;

        case LOOKER_STATE_UPDATE_VALUE:
            if (var_number >= 0)
            {
                ((char *) looker_var[var_number].addr)[pos] = c;
                looker_var[var_number].value[pos] = c;
                pos++;
                if (looker_var[var_number].type == LOOKER_VAR_STRING)
                {
                    if (!c)
                        state = LOOKER_STATE_START;
                }
                else
                    if (looker_var[var_number].size == pos)
                        state = LOOKER_STATE_START;
            }
            else
                var_number = c;
            break;

        default:
            return LOOKER_EXIT_BAD_STATE;
            break;
        }

    }
    return LOOKER_EXIT_SUCCESS;
}

void looker_destroy(void)
{
#ifdef LOOKER_USE_MALLOC
    if (looker_var)
    {
        free(looker_var);
        looker_var = NULL;
    }
#endif //LOOKER_USE_MALLOC
}

