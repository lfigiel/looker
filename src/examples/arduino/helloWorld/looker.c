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
#include <stdint.h>
#include "looker.h"
#include "looker_wifi.h"
#include "checksum.h"
#include "looker_stubs.h"

enum {
    NOT_UPDATED = 0,
    UPDATED_FROM_MASTER,
    UPDATED_FROM_SLAVE
};

#define ACK_TIMEOUT 1000
#define RESET_POSTPOND 500

//todo: change to .c
#include "msg.h"

typedef enum {
    MASTER_STATE_RESET = 0,
    MASTER_STATE_RESET_ACK,
    MASTER_STATE_SYNC
} master_state_t;

typedef struct {
    const char *name;
    void *value_current;
    char value_old[LOOKER_VAR_VALUE_SIZE];
    char value_slave[LOOKER_VAR_VALUE_SIZE];
    char value_update;
    unsigned char size;
    unsigned char type;
    looker_label_t label;
#ifdef LOOKER_STYLE_DYNAMIC
    char *style_current;
    char style_old[LOOKER_VAR_STYLE_SIZE];
    char style_update;
#else
    const char *style_current;
#endif //LOOKER_STYLE_DYNAMIC
} var_t;

//globals
static master_state_t master_state = MASTER_STATE_RESET;
static looker_slave_state_t slave_state = LOOKER_SLAVE_STATE_UNKNOWN;
#ifdef LOOKER_USE_MALLOC
static var_t *var = NULL;
#else
static var_t var[LOOKER_VAR_COUNT_MAX];
#endif //LOOKER_USE_MALLOC
static size_t var_cnt;      //total variables count
static size_t var_reg_cnt;  //total variables registered

//prototypes
static void master_state_change(master_state_t state);
static looker_exit_t payload_process(void);
static looker_exit_t var_update_master(void);
static looker_exit_t var_update_slave(void);

looker_slave_state_t looker_slave_state(void)
{
    return slave_state;
}

static void master_state_change(master_state_t state)
{
    switch(state) {
        case MASTER_STATE_RESET:
            master_state = state;
            slave_state = LOOKER_SLAVE_STATE_UNKNOWN;
        break;

        case MASTER_STATE_RESET_ACK:
            master_state = state;
            slave_state = LOOKER_SLAVE_STATE_UNKNOWN;
        break;

        case MASTER_STATE_SYNC:
            master_state = state;
        break;
        default:
        break;
    }
}

static looker_exit_t var_register(void)
{
    unsigned char ack;

    while (var_cnt > var_reg_cnt)
    {
        do {
            size_t i = var_reg_cnt;
            msg.payload_size = 0;
            msg.payload[msg.payload_size++] = COMMAND_REGISTER;
            msg.payload[msg.payload_size++] = i;

            msg.payload[msg.payload_size++] = var[i].size;
            msg.payload[msg.payload_size++] = var[i].type;

            memcpy(&msg.payload[msg.payload_size], var[i].value_current, var[i].size);
            msg.payload_size += var[i].size;

            //name can be NULL
            if (var[i].name)
            {
                memcpy(&msg.payload[msg.payload_size], var[i].name, strlen(var[i].name)+1);
                msg.payload_size += strlen(var[i].name)+1;
            }
            else
                msg.payload[msg.payload_size++] = 0;

            msg.payload[msg.payload_size++] = var[i].label;

            //style can be NULL
            if (var[i].style_current)
            {
                memcpy(&msg.payload[msg.payload_size], var[i].style_current, strlen(var[i].style_current)+1);
                msg.payload_size += strlen(var[i].style_current)+1;
            }
            else
                msg.payload[msg.payload_size++] = 0;

            msg_send();
            
            //wait for ack
            size_t j;
            for (j=0; j<ACK_TIMEOUT; j++)
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
            if (j >= ACK_TIMEOUT)
            {
                PRINTF1("timeout: %lu\n", j);
                return LOOKER_EXIT_TIMEOUT;
            }
            else
                PRINTF1("time: %lu\n", j);

        } while (ack != ACK_SUCCESS);

        var_reg_cnt++;
    }
    return LOOKER_EXIT_SUCCESS;
}

static looker_exit_t var_update_slave(void)
{
    size_t i, j;
    unsigned char ack;

    for (i=0; i<var_cnt; i++)
    {
        //value
        if (memcmp(var[i].value_old, (const void *) var[i].value_current, var[i].size) != 0)
        {
            var[i].value_update = UPDATED_FROM_MASTER;

            msg.payload_size = 0;
            msg.payload[msg.payload_size++] = COMMAND_UPDATE_VALUE;
            msg.payload[msg.payload_size++] = i;

            memcpy(&msg.payload[msg.payload_size], var[i].value_current, var[i].size);
            msg.payload_size += var[i].size;

            do
            {
                msg_send();
                
                //wait for ack
                for (j=0; j<ACK_TIMEOUT; j++)
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
                if (j >= ACK_TIMEOUT)
                {
                    PRINTF1("timeout: %lu\n", j);
                    return LOOKER_EXIT_TIMEOUT;
                }
                else
                    PRINTF1("time: %lu\n", j);


            } while (ack != ACK_SUCCESS);

        }
        else
            var[i].value_update = NOT_UPDATED;



        //style
#ifdef LOOKER_STYLE_DYNAMIC
        if (var[i].style_current)
        {
            if (strcmp(var[i].style_current, var[i].style_old) != 0)
                var[i].style_update = UPDATED_FROM_MASTER;
            else
                var[i].style_update = NOT_UPDATED;
        }
        else
        if (var[i].style_old[0])
            var[i].style_update = UPDATED_FROM_MASTER;
        else
            var[i].style_update = NOT_UPDATED;

        if (var[i].style_update)
        {
            msg.payload_size = 0;
            msg.payload[msg.payload_size++] = COMMAND_UPDATE_STYLE;
            msg.payload[msg.payload_size++] = i;

            if (var[i].style_current)
            {
                strcpy((char *) &msg.payload[msg.payload_size], var[i].style_current);
                msg.payload_size += strlen(var[i].style_current);
            }
            else
                msg.payload[msg.payload_size++] = 0;

            do
            {
                msg_send();
                
                //wait for ack
                for (j=0; j<ACK_TIMEOUT; j++)
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
                if (j >= ACK_TIMEOUT)
                {
                    PRINTF1("timeout: %lu\n", j);
                    return LOOKER_EXIT_TIMEOUT;
                }
                else
                    PRINTF1("time: %lu\n", j);


            } while (ack != ACK_SUCCESS);
        }
#endif //LOOKER_STYLE_DYNAMIC

    }
    return LOOKER_EXIT_SUCCESS;
}

looker_exit_t looker_init(const char *ssid, const char *pass, const char *domain)
{
#ifdef LOOKER_SANITY_TEST
    if ((LOOKER_VAR_COUNT_MAX > LOOKER_WIFI_VAR_COUNT_MAX) ||
        (LOOKER_VAR_VALUE_SIZE > LOOKER_WIFI_VAR_VALUE_SIZE) ||
        (LOOKER_VAR_NAME_SIZE > LOOKER_WIFI_VAR_NAME_SIZE) ||
        (LOOKER_VAR_STYLE_SIZE > LOOKER_WIFI_VAR_STYLE_SIZE))
        return LOOKER_EXIT_NO_MEMORY;
#endif //LOOKER_SANITY_TEST

//empty rx buffer
//unsigned char rx;
//while (looker_data_available())
//    looker_get(&rx, 1);

    looker_exit_t err;
    if (!ssid)
        return LOOKER_EXIT_BAD_PARAMETER;
    if (pass)
    {
        if ((err = looker_reg(NULL, (volatile void *) pass, strlen(pass), LOOKER_TYPE_STRING, LOOKER_LABEL_PASS, NULL)) != LOOKER_EXIT_SUCCESS)
            return err;
    }
    if (domain)
    {
        if ((err = looker_reg(NULL, (volatile void *) domain, strlen(domain), LOOKER_TYPE_STRING, LOOKER_LABEL_DOMAIN, NULL)) != LOOKER_EXIT_SUCCESS)
            return err;
    }

    //ssid at the end
    if ((err = looker_reg(NULL, (volatile void *) ssid, strlen(ssid), LOOKER_TYPE_STRING, LOOKER_LABEL_SSID, NULL)) != LOOKER_EXIT_SUCCESS)
        return err;

    return LOOKER_EXIT_SUCCESS;
}

looker_exit_t looker_reg(const char *name, volatile void *addr, int size, looker_type_t type, looker_label_t label, STYLE_TYPE style)
{
    if (type == LOOKER_TYPE_STRING)
        size = strlen((const char *) addr) + 1;

#ifdef LOOKER_SANITY_TEST
    if (var_cnt >= LOOKER_VAR_COUNT_MAX)
        return LOOKER_EXIT_NO_MEMORY;

    if (type >= LOOKER_TYPE_LAST)
        return LOOKER_EXIT_BAD_PARAMETER;

    if (size > LOOKER_VAR_VALUE_SIZE)
        return LOOKER_EXIT_NO_MEMORY;

    if ((type >= LOOKER_TYPE_FLOAT_0) && (type <= LOOKER_TYPE_FLOAT_4) && (size != sizeof(float)) && (size != sizeof(double)))
        return LOOKER_EXIT_BAD_PARAMETER;

    if (name && (strlen(name) + 1 > LOOKER_VAR_NAME_SIZE))
        return LOOKER_EXIT_BAD_PARAMETER;

    if (label >= LOOKER_LABEL_LAST)
        return LOOKER_EXIT_BAD_PARAMETER;

    if (style && (strlen(style) + 1 > LOOKER_WIFI_VAR_STYLE_SIZE))
        return LOOKER_EXIT_BAD_PARAMETER;

    //one ssid, one pass, one domain
    if ((label == LOOKER_LABEL_SSID) || (label == LOOKER_LABEL_PASS) || (label == LOOKER_LABEL_DOMAIN))
    {
        size_t i;
        for (i=0; i<var_cnt; i++)
            if (var[i].label == label)
                return LOOKER_EXIT_BAD_PARAMETER;
    }
#endif //LOOKER_SANITY_TEST

#ifdef LOOKER_USE_MALLOC
    var_t *p;
    if ((p = (var_t *) realloc(var, (size_t) ((var_cnt + 1) * sizeof(var_t)))) == NULL)
    {
        //looker_destroy() will free old memory block pointed by var 
        return LOOKER_EXIT_NO_MEMORY;
    }
    var = p;
#endif //LOOKER_USE_MALLOC

    var[var_cnt].value_current = (void *) addr;
    memcpy(var[var_cnt].value_old, (const void *) addr, size);
    var[var_cnt].size = size;
    var[var_cnt].type = type;
    var[var_cnt].label = label;
    var[var_cnt].style_current = style;
#ifdef LOOKER_STYLE_DYNAMIC
    if (style)
        strcpy(var[var_cnt].style_old, style);
    else
        var[var_cnt].style_old[0] = 0;
#endif //LOOKER_STYLE_DYNAMIC
    var[var_cnt].name = name;
    var_cnt++;

    return LOOKER_EXIT_SUCCESS;
}

void looker_update(void)
{
    size_t i, j;

    switch (master_state) {
        case MASTER_STATE_RESET:
            //postpond
            j = 0;
            while (++j < RESET_POSTPOND)
                looker_delay();
            for (i=0; i<var_cnt; i++)
            {
                var[i].value_update = NOT_UPDATED;
#ifdef LOOKER_STYLE_DYNAMIC
                var[i].style_update = NOT_UPDATED;
#endif //LOOKER_STYLE_DYNAMIC
            }
            var_reg_cnt = 0;
            msg.pos = POS_PREFIX;
            command_send(COMMAND_RESET);
            master_state_change(MASTER_STATE_RESET_ACK);
        break;

        case MASTER_STATE_RESET_ACK:
            if (looker_data_available())
            {
                if (ack_get() == ACK_SUCCESS)
                    master_state_change(MASTER_STATE_SYNC);
                else
                {
                    master_state_change(MASTER_STATE_RESET);
                    break;
                }
            }
            else
            {
                master_state_change(MASTER_STATE_RESET);
                break;
            }
        //no break

        case MASTER_STATE_SYNC:
            //register slave
            if (var_register())
            {
                master_state_change(MASTER_STATE_RESET);
                break;
            }

            //only if slave is connected
            if (slave_state == LOOKER_SLAVE_STATE_CONNECTED)
            {
                //update slave
                if (var_update_slave())
                {
                    master_state_change(MASTER_STATE_RESET);
                    break;
                }
            }

            //update master
            if (var_update_master())
            {
                master_state_change(MASTER_STATE_RESET);
                break;
            }

            //only if slave is connected
            if (slave_state == LOOKER_SLAVE_STATE_CONNECTED)
            {
                //validate
                for (i=0; i<var_cnt; i++)
                {
                    //value
                    if (var[i].value_update == UPDATED_FROM_MASTER)
                    {
                        PRINTF("Update from master MMMMMMM\n");
                        memcpy(var[i].value_old, (const void *) var[i].value_current, var[i].size);
                    }
                    else if (var[i].value_update == UPDATED_FROM_SLAVE)
                    {
                        PRINTF("Update from slave SSSSSSS\n");
                        memcpy(var[i].value_current, var[i].value_slave, var[i].size);
                        memcpy(var[i].value_old, var[i].value_slave, var[i].size);
                    }

                    //style
#ifdef LOOKER_STYLE_DYNAMIC
                    if (var[i].style_update == UPDATED_FROM_MASTER)
                    {
                        if (var[i].style_current)
                            strcpy(var[i].style_old, var[i].style_current);
                        else
                            var[i].style_old[0] = 0;
                    }
#endif //LOOKER_STYLE_DYNAMIC
                }
            }
        break;

        default:
        break;
    }
}

static looker_exit_t var_update_master(void)
{
    size_t j;
    unsigned char ack;

    do
    {
        command_send(COMMAND_UPDATE_GET);
        
        //wait for ack
        for (j=0; j<ACK_TIMEOUT; j++)
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
        if (j >= ACK_TIMEOUT)
        {
            PRINTF1("timeout: %lu\n", j);
            return LOOKER_EXIT_TIMEOUT;
        }
        else
            PRINTF1("time: %lu\n", j);

    } while (ack != ACK_SUCCESS);

    do
    {
        //wait for update
        for (j=0; j<ACK_TIMEOUT; j++)
        {
            looker_delay();
            if (looker_data_available())
            {
                if (msg_get() != LOOKER_EXIT_SUCCESS)
                {
                    ack_send(ACK_FAILURE);
                    j = 0;
                }
            }
            if (msg_complete())
                break;
        }

        //timeout
        if (j >= ACK_TIMEOUT)
        {
            PRINTF1("timeout: %lu\n", j);
            return LOOKER_EXIT_TIMEOUT;
        }
        else
            PRINTF1("time: %lu\n", j);

        payload_process();

    } while (msg.payload[0] == COMMAND_UPDATE_VALUE);

    if (msg.payload[0] != COMMAND_STATUS)
        return LOOKER_EXIT_BAD_COMMAND;

    return LOOKER_EXIT_SUCCESS;
}

static looker_exit_t payload_process(void)
{
    switch ((unsigned char) msg.payload[0]) {
        case COMMAND_UPDATE_VALUE:
            PRINTF("<-UPDATE_VALUE -----------\n");
            var[msg.payload[1]].value_update = UPDATED_FROM_SLAVE;
            memcpy(var[msg.payload[1]].value_slave, &msg.payload[2], var[msg.payload[1]].size);
            ack_send(ACK_SUCCESS);
        break;

        case COMMAND_STATUS:
            PRINTF("<-STATUS\n");
            slave_state = msg.payload[1];
#ifdef DEBUG
            PRINTF("  slave_state: ");
            switch (slave_state) {
                case LOOKER_SLAVE_STATE_UNKNOWN:
                    PRINTF("UNKNOWN\n");
                break;
                case LOOKER_SLAVE_STATE_DISCONNECTED:
                    PRINTF("DISCONNECTED\n");
                break;
                case LOOKER_SLAVE_STATE_CONNECTING:
                    PRINTF("CONNECTING\n");
                break;
                case LOOKER_SLAVE_STATE_CONNECTED:
                    PRINTF("CONNECTED\n");
                break;
                default:
                break;
            }
#endif //DEBUG
            ack_send(ACK_SUCCESS);
        break;

        default:
            PRINTF1("Error: payload_process: bad command: %u\n", (unsigned char) msg.payload[0]);
            ack_send(ACK_FAILURE);
            return LOOKER_EXIT_BAD_COMMAND;
        break;
    }
    return LOOKER_EXIT_SUCCESS;
}

void looker_destroy(void)
{
#ifdef LOOKER_USE_MALLOC
    if (var)
    {
        free(var);
        var = NULL;
    }
#endif //LOOKER_USE_MALLOC
}

