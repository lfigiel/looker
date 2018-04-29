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
#include "looker_master.h"
#include "msg.h"
#include "looker_stubs.h"

enum {
    NOT_UPDATED = 0,
    UPDATED_FROM_MASTER,
    UPDATED_FROM_SLAVE
};

#define RESET_POSTPOND 500  //ms

typedef enum {
    MASTER_STATE_RESET = 0,
    MASTER_STATE_RESET_ACK,
    MASTER_STATE_SYNC
} master_state_t;

#pragma pack(1)
typedef struct {
    const char *name;
    void *value_current;
    char value_old[LOOKER_MASTER_VAR_VALUE_SIZE];
    char value_slave[LOOKER_MASTER_VAR_VALUE_SIZE];
    char value_update;
    unsigned char size;
    unsigned char type;
    looker_label_t label;
#if LOOKER_MASTER_STYLE == LOOKER_STYLE_FIXED
    const char *style_current;
#elif LOOKER_MASTER_STYLE == LOOKER_STYLE_VARIABLE
    const char **style_current;
    const char *style_old;
#endif //LOOKER_MASTER_STYLE == LOOKER_STYLE_FIXED
    unsigned char style_update;
} var_t;
#pragma pack()

//globals
static master_state_t master_state = MASTER_STATE_RESET;
static looker_slave_state_t slave_state = LOOKER_SLAVE_STATE_UNKNOWN;
#ifdef LOOKER_MASTER_USE_MALLOC
static var_t *var = NULL;
#else
static var_t var[LOOKER_MASTER_VAR_COUNT];
#endif //LOOKER_MASTER_USE_MALLOC
static size_t var_cnt;      //total variables count
static size_t var_reg_cnt;  //total variables registered
static looker_network_t network = LOOKER_NETWORK_DO_NOTHING;
static msg_t msg;

//prototypes
static void master_state_change(master_state_t state);
static looker_exit_t payload_process(msg_t *msg);
static looker_exit_t slave_status_get(void);
static looker_exit_t slave_var_get(void);
static looker_exit_t slave_var_reg(void);
static looker_exit_t slave_var_set(void);

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

static looker_exit_t slave_status_get(void)
{
    msg_t msg;
    looker_exit_t err;

    msg_begin(&msg, COMMAND_STATUS_GET);
    msg_send(&msg);

    if ((err = msg_get(&msg)) != LOOKER_EXIT_SUCCESS)
        return err;
    if (msg.payload[0] != RESPONSE_STATUS)
        return LOOKER_EXIT_WRONG_RESPONSE;

    return payload_process(&msg);
}

static looker_exit_t slave_var_get(void)
{
    msg_t msg;
    looker_exit_t err;

    for(;;)
    {
        msg_begin(&msg, COMMAND_VALUE_GET);
        msg_send(&msg);

        if ((err = msg_get(&msg)) == LOOKER_EXIT_SUCCESS)
        {
            if (msg.payload[0] == RESPONSE_VALUE_NO_MORE)
                return LOOKER_EXIT_SUCCESS;
            else if (msg.payload[0] != RESPONSE_VALUE)
                return LOOKER_EXIT_WRONG_RESPONSE;
        }
        else
        {
            msg_begin(&msg, COMMAND_VALUE_REPEAT);
            msg_send(&msg);

            if ((err = msg_get(&msg)) == LOOKER_EXIT_SUCCESS)
            {
                if (msg.payload[0] == RESPONSE_VALUE_NO_MORE)
                    return LOOKER_EXIT_SUCCESS;
                else if (msg.payload[0] != RESPONSE_VALUE)
                    return LOOKER_EXIT_WRONG_RESPONSE;
            }
            else
                return err;
        }

        if ((err = payload_process(&msg)) != LOOKER_EXIT_SUCCESS)
            return err;
    }
    return LOOKER_EXIT_SUCCESS;
}

static looker_exit_t slave_var_reg(void)
{
    unsigned char ack;
    looker_exit_t err;

    while (var_cnt > var_reg_cnt)
    {
        size_t i = var_reg_cnt;
        msg_begin(&msg, COMMAND_VAR_REG);

        msg.payload[msg.payload_size++] = i;
        msg.payload[msg.payload_size++] = var[i].size;
        msg.payload[msg.payload_size++] = var[i].type;
        strcpy((char *) &msg.payload[msg.payload_size], var[i].name);
        msg.payload_size += strlen(var[i].name)+1;
        msg.payload[msg.payload_size++] = var[i].label;

        do {
            msg_send(&msg);
            
            if ((err = ack_get(&ack)) != LOOKER_EXIT_SUCCESS)
                return err;

        } while (ack != RESPONSE_ACK_SUCCESS);

        //reset value_old
        //value will be transferred during update (unless it is zero)
        if (var[i].type == LOOKER_TYPE_STRING)
            var[i].value_old[0] = 0;
        else
            memset(var[i].value_old, 0, var[i].size);

        //style will be transferred during update (unless it is zero)
#if LOOKER_MASTER_STYLE == LOOKER_STYLE_FIXED
        if (var[i].style_current)
            var[i].style_update = UPDATED_FROM_MASTER;
#elif LOOKER_MASTER_STYLE == LOOKER_STYLE_VARIABLE
        //reset style_old
        var[i].style_old = NULL;
#endif //LOOKER_MASTER_STYLE == LOOKER_STYLE_FIXED
        var_reg_cnt++;
    }
    return LOOKER_EXIT_SUCCESS;
}

static looker_exit_t slave_var_set(void)
{
    size_t i;
    unsigned char ack;
    looker_exit_t err;

    for (i=0; i<var_cnt; i++)
    {
        //skip some vars
        if ((var[i].label == LOOKER_LABEL_SSID) || (var[i].label == LOOKER_LABEL_PASS) || (var[i].label == LOOKER_LABEL_DOMAIN))
            continue;

        //value
        if (var[i].type == LOOKER_TYPE_STRING)
        {
#ifdef LOOKER_MASTER_SANITY_TEST
            if ((strlen((const char *) var[i].value_current) + 1) > LOOKER_MASTER_VAR_VALUE_SIZE)
                return LOOKER_EXIT_NO_MEMORY;
#endif //LOOKER_MASTER_SANITY_TEST
            if (strcmp(var[i].value_old, (const char *) var[i].value_current) != 0)
            {
                var[i].value_update = UPDATED_FROM_MASTER;

                msg_begin(&msg, COMMAND_VALUE_SET);
                msg.payload[msg.payload_size++] = i;

                strcpy((char *) &msg.payload[msg.payload_size], (const char *) var[i].value_current);
                msg.payload_size += (strlen((const char *) var[i].value_current) + 1);

                do
                {
                    msg_send(&msg);
                    if ((err = ack_get(&ack)) != LOOKER_EXIT_SUCCESS)
                        return err;
                } while (ack != RESPONSE_ACK_SUCCESS);
            }
            else
                var[i].value_update = NOT_UPDATED;
        }
        else if (memcmp(var[i].value_old, (const char *) var[i].value_current, var[i].size) != 0)
        {
            var[i].value_update = UPDATED_FROM_MASTER;

            msg_begin(&msg, COMMAND_VALUE_SET);
            msg.payload[msg.payload_size++] = i;

            memcpy(&msg.payload[msg.payload_size], var[i].value_current, var[i].size);
            msg.payload_size += var[i].size;

            do
            {
                msg_send(&msg);
                if ((err = ack_get(&ack)) != LOOKER_EXIT_SUCCESS)
                    return err;
            } while (ack != RESPONSE_ACK_SUCCESS);
        }
        else
            var[i].value_update = NOT_UPDATED;

        //style
#if LOOKER_MASTER_STYLE == LOOKER_STYLE_VARIABLE
        if ((var[i].style_current) && (*var[i].style_current))
        {
            if (*var[i].style_current != var[i].style_old)
                var[i].style_update = UPDATED_FROM_MASTER;
            else
                var[i].style_update = NOT_UPDATED;
        }
        else if (var[i].style_old)
            var[i].style_update = UPDATED_FROM_MASTER;
        else
            var[i].style_update = NOT_UPDATED;
#endif //LOOKER_MASTER_STYLE == LOOKER_STYLE_VARIABLE

#if ((LOOKER_MASTER_STYLE == LOOKER_STYLE_FIXED) || (LOOKER_MASTER_STYLE == LOOKER_STYLE_VARIABLE))
        if (var[i].style_update != NOT_UPDATED)
        {
            msg_begin(&msg, COMMAND_STYLE_SET);
            msg.payload[msg.payload_size++] = i;

#if LOOKER_MASTER_STYLE == LOOKER_STYLE_FIXED
            if (var[i].style_current)
            {
                strcpy((char *) &msg.payload[msg.payload_size], var[i].style_current);
//todo: double check this + 1
                msg.payload_size += (strlen(var[i].style_current) + 1);
            }
#else
            if ((var[i].style_current) && (*var[i].style_current))
            {
                strcpy((char *) &msg.payload[msg.payload_size], *var[i].style_current);
//todo: double check this + 1
                msg.payload_size += (strlen(*var[i].style_current) + 1);
            }
#endif //LOOKER_MASTER_STYLE == LOOKER_STYLE_FIXED
            else
                msg.payload[msg.payload_size++] = 0;

            do
            {
                msg_send(&msg);
                if ((err = ack_get(&ack)) != LOOKER_EXIT_SUCCESS)
                    return err;
            } while (ack != RESPONSE_ACK_SUCCESS);
        }
#endif //((LOOKER_MASTER_STYLE == LOOKER_STYLE_FIXED) || (LOOKER_MASTER_STYLE == LOOKER_STYLE_VARIABLE))
    }
    return LOOKER_EXIT_SUCCESS;
}

void looker_init(void)
{
    //reset master
    var_cnt = 0;
    var_reg_cnt = 0;

    //this will also reset slave vars through COMMAND_RESET
    master_state_change(MASTER_STATE_RESET);
}

looker_exit_t looker_connect(const char *ssid, const char *pass, const char *domain)
{
    looker_exit_t err;

    looker_destroy();
    looker_init();

    if (ssid)
    {
        if ((err = looker_reg(ssid, NULL, 0, LOOKER_TYPE_STRING, LOOKER_LABEL_SSID, NULL)) != LOOKER_EXIT_SUCCESS)
            return err;
    }
    else
        return LOOKER_EXIT_WRONG_PARAMETER;   //ssid is a must

    if (pass)
    {
        if ((err = looker_reg(pass, NULL, 0, LOOKER_TYPE_STRING, LOOKER_LABEL_PASS, NULL)) != LOOKER_EXIT_SUCCESS)
            return err;
    }
    if (domain)
    {
        if ((err = looker_reg(domain, NULL, 0, LOOKER_TYPE_STRING, LOOKER_LABEL_DOMAIN, NULL)) != LOOKER_EXIT_SUCCESS)
            return err;
    }

    network = LOOKER_NETWORK_RECONNECT;

    return LOOKER_EXIT_SUCCESS;
}

void looker_disconnect(void)
{
    network = LOOKER_NETWORK_STAY_DISCONNECTED;
}

looker_exit_t looker_reg(const char *name, volatile void *addr, int size, looker_type_t type, looker_label_t label, looker_style_t style)
{
    if ((type == LOOKER_TYPE_STRING) && (label != LOOKER_LABEL_SSID) && (label != LOOKER_LABEL_PASS) && (label != LOOKER_LABEL_DOMAIN))
        size = strlen((const char *) addr) + 1;

#ifdef LOOKER_MASTER_SANITY_TEST
    if (var_cnt >= LOOKER_MASTER_VAR_COUNT)
        return LOOKER_EXIT_NO_MEMORY;

    if (!name)
        return LOOKER_EXIT_WRONG_PARAMETER;

    if (strlen(name) + 1 > LOOKER_MASTER_VAR_NAME_SIZE)
        return LOOKER_EXIT_NO_MEMORY;

    if (size > LOOKER_MASTER_VAR_VALUE_SIZE)
        return LOOKER_EXIT_NO_MEMORY;

    if (type >= LOOKER_TYPE_LAST)
        return LOOKER_EXIT_WRONG_PARAMETER;

    if ((type >= LOOKER_TYPE_FLOAT_0) && (type <= LOOKER_TYPE_FLOAT_4) && (size != sizeof(float)) && (size != sizeof(double)))
        return LOOKER_EXIT_WRONG_PARAMETER;

    if (label >= LOOKER_LABEL_LAST)
        return LOOKER_EXIT_WRONG_PARAMETER;

#if LOOKER_MASTER_STYLE == LOOKER_STYLE_VARIABLE
    if (style && *style &&(strlen(*style) + 1 > LOOKER_VAR_STYLE_SIZE))
#else
    if (style &&(strlen(style) + 1 > LOOKER_VAR_STYLE_SIZE))
#endif //LOOKER_MASTER_STYLE == LOOKER_STYLE_VARIABLE
        return LOOKER_EXIT_WRONG_PARAMETER;

    size_t i;
    //one ssid, one pass, one domain
    if ((label == LOOKER_LABEL_SSID) || (label == LOOKER_LABEL_PASS) || (label == LOOKER_LABEL_DOMAIN))
    {
        for (i=0; i<var_cnt; i++)
            if (var[i].label == label)
                return LOOKER_EXIT_WRONG_PARAMETER;
    }
    else
    {
        //name cannot be duplicated
        for (i=0; i<var_cnt; i++)
            if ((var[i].name) && (!strcmp(var[i].name, name)))
                return LOOKER_EXIT_WRONG_PARAMETER;
    }
#endif //LOOKER_MASTER_SANITY_TEST

#ifdef LOOKER_MASTER_USE_MALLOC
    var_t *p;
    if ((p = (var_t *) realloc(var, (size_t) ((var_cnt + 1) * sizeof(var_t)))) == NULL)
    {
        //looker_destroy() will free old memory block pointed by var 
        return LOOKER_EXIT_NO_MEMORY;
    }
    var = p;
#endif //LOOKER_MASTER_USE_MALLOC

    var[var_cnt].name = name;
    var[var_cnt].value_current = (void *) addr;
    if (type == LOOKER_TYPE_STRING)
        var[var_cnt].size = 0;
    else
        var[var_cnt].size = size;
    var[var_cnt].type = type;
    var[var_cnt].label = label;
#if ((LOOKER_MASTER_STYLE == LOOKER_STYLE_FIXED) || (LOOKER_MASTER_STYLE == LOOKER_STYLE_VARIABLE))
    var[var_cnt].style_current = style;
#endif //((LOOKER_MASTER_STYLE == LOOKER_STYLE_FIXED) || (LOOKER_MASTER_STYLE == LOOKER_STYLE_VARIABLE))
    var_cnt++;

    return LOOKER_EXIT_SUCCESS;
}

looker_exit_t looker_update(void)
{
    size_t i, j;
    unsigned char ack;
    looker_exit_t err;

    switch (master_state) {
        case MASTER_STATE_RESET:
            PRINT("Master state: MASTER_STATE_RESET\n");
            //re-connection takes time so reset does not disrupt connection
            j = 0;
            while (++j < RESET_POSTPOND)
                looker_delay_1ms();
            msg_begin(&msg, COMMAND_RESET);
            msg_send(&msg);
            master_state_change(MASTER_STATE_RESET_ACK);
        break;

        case MASTER_STATE_RESET_ACK:
            if ((err = ack_get(&ack)) != LOOKER_EXIT_SUCCESS)
            {
                master_state_change(MASTER_STATE_RESET);
                return err;
            }

            if (ack == RESPONSE_ACK_SUCCESS)
            {
                //get slave status to make sure slave is synced up
                if ((err = slave_status_get()) != LOOKER_EXIT_SUCCESS)
                {
                    master_state_change(MASTER_STATE_RESET);
                    return err;
                }
                //reset variables here
                var_reg_cnt = 0;
                master_state_change(MASTER_STATE_SYNC);
            }
            else
            {
                master_state_change(MASTER_STATE_RESET);
                return LOOKER_EXIT_ACK_FAILURE;
            }
        //no break

        case MASTER_STATE_SYNC:
            //reg vars
            if ((err = slave_var_reg()) != LOOKER_EXIT_SUCCESS)
            {
                master_state_change(MASTER_STATE_RESET);
                return err;
            }

            //get slave status
            if ((err = slave_status_get()) != LOOKER_EXIT_SUCCESS)
            {
                master_state_change(MASTER_STATE_RESET);
                return err;
            }

            //only if slave is connected
            if (slave_state == LOOKER_SLAVE_STATE_CONNECTED)
            {
                //update slave
                if ((err = slave_var_set()) != LOOKER_EXIT_SUCCESS)
                {
                    master_state_change(MASTER_STATE_RESET);
                    return err;
                }

                //update master
                if ((err = slave_var_get()) != LOOKER_EXIT_SUCCESS)
                {
                    master_state_change(MASTER_STATE_RESET);
                    return err;
                }

                //validate
                for (i=0; i<var_cnt; i++)
                {
                    //value
                    if (var[i].value_update == UPDATED_FROM_MASTER)
                    {
                        if (var[i].type == LOOKER_TYPE_STRING)
                            strcpy(var[i].value_old, (const char *) var[i].value_current);
                        else
                            memcpy(var[i].value_old, (const void *) var[i].value_current, var[i].size);
#ifdef DEBUG_UPDATE
                        PRINT("Master updated value\n");
                        PRINT("  name: ");
                        if (strlen((char *) var[i].name))
                        {
                            PRINT((char *) var[i].name);PRINT("\n");
                        }
                        else
                        {
                            PRINT("NULL\n");
                        }
#endif //DEBUG_UPDATE
                    }
                    else if (var[i].value_update == UPDATED_FROM_SLAVE)
                    {
                        if (var[i].type == LOOKER_TYPE_STRING)
                        {
                            strcpy(var[i].value_current, (const char *) var[i].value_slave);
                            strcpy(var[i].value_old, (const char *) var[i].value_slave);
                        }
                        else
                        {
                            memcpy(var[i].value_current, var[i].value_slave, var[i].size);
                            memcpy(var[i].value_old, var[i].value_slave, var[i].size);
                        }
#ifdef DEBUG_UPDATE
                        PRINT("Slave updated value\n");
                        PRINT("  name: ");
                        if (strlen((char *) var[i].name))
                        {
                            PRINT((char *) var[i].name);PRINT("\n");
                        }
                        else
                        {
                            PRINT("NULL\n");
                        }
#endif //DEBUG_UPDATE
                    }

                    //style
                    if (var[i].style_update == UPDATED_FROM_MASTER)
                    {
#if LOOKER_MASTER_STYLE == LOOKER_STYLE_VARIABLE
                            var[i].style_old = *var[i].style_current;
#endif //LOOKER_MASTER_STYLE == LOOKER_STYLE_VARIABLE
                        var[i].style_update = NOT_UPDATED;
#ifdef DEBUG_UPDATE
                        PRINT("Master updated style\n");
                        //var with style should have name
                        PRINT("  name: ");
                            PRINT((char *) var[i].name);PRINT("\n");
#endif //DEBUG_UPDATE
                    }
                }
            }

#ifdef DEBUG_NETWORK
            PRINT("  network: ");
            switch (network) {
                case LOOKER_NETWORK_DO_NOTHING:
                    PRINT("LOOKER_NETWORK_DO_NOTHING\n");
                break;

                case LOOKER_NETWORK_STAY_CONNECTED:
                    PRINT("LOOKER_NETWORK_STAY_CONNECTED\n");
                break;

                case LOOKER_NETWORK_STAY_DISCONNECTED:
                    PRINT("LOOKER_NETWORK_STAY_DISCONNECTED\n");
                break;

                case LOOKER_NETWORK_RECONNECT:
                    PRINT("LOOKER_NETWORK_RECONNECT\n");
                break;

                case LOOKER_NETWORK_RECONNECT_2:
                    PRINT("LOOKER_NETWORK_RECONNECT_2\n");
                break;
                default:
                break;
            }
#endif //DEBUG_NETWORK

//todo: move to slave
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
                            msg_begin(&msg, COMMAND_CONNECT);
                            do
                            {
                                msg_send(&msg);
                                if ((err = ack_get(&ack)) != LOOKER_EXIT_SUCCESS)
                                {
                                    master_state_change(MASTER_STATE_RESET);
                                    return err;
                                }
                            } while (ack != RESPONSE_ACK_SUCCESS);
                        break;
                        default:
                        break;
                    }
                break;

                case LOOKER_NETWORK_STAY_DISCONNECTED:
                    switch (slave_state) {
                        case LOOKER_SLAVE_STATE_CONNECTING:
                        case LOOKER_SLAVE_STATE_CONNECTED:
                            msg_begin(&msg, COMMAND_DISCONNECT);
                            do
                            {
                                msg_send(&msg);
                                if ((err = ack_get(&ack)) != LOOKER_EXIT_SUCCESS)
                                {
                                    master_state_change(MASTER_STATE_RESET);
                                    return err;
                                }
                            } while (ack != RESPONSE_ACK_SUCCESS);
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
                            msg_begin(&msg, COMMAND_DISCONNECT);
                            do
                            {
                                msg_send(&msg);
                                if ((err = ack_get(&ack)) != LOOKER_EXIT_SUCCESS)
                                {
                                    master_state_change(MASTER_STATE_RESET);
                                    return err;
                                }
                            } while (ack != RESPONSE_ACK_SUCCESS);
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
                            msg_begin(&msg, COMMAND_CONNECT);
                            do
                            {
                                msg_send(&msg);
                                if ((err = ack_get(&ack)) != LOOKER_EXIT_SUCCESS)
                                {
                                    master_state_change(MASTER_STATE_RESET);
                                    return err;
                                }
                            } while (ack != RESPONSE_ACK_SUCCESS);
                        break;
                        default:
                        break;
                    }
                break;
                default:
                break;
            }
        break;

        default:
            PRINTLN2("Error: looker_update: bad master state: ", master_state);
            master_state_change(MASTER_STATE_RESET);
            return LOOKER_EXIT_WRONG_STATE;
        break;
    }
    return LOOKER_EXIT_SUCCESS;
}

static looker_exit_t payload_process(msg_t *msg)
{
    switch ((unsigned char) msg->payload[0]) {
        case RESPONSE_VALUE:
            if (var[msg->payload[1]].type == LOOKER_TYPE_STRING)
            {
#ifdef LOOKER_MASTER_SANITY_TEST
                if ((strlen((const char *) &msg->payload[2]) + 1 ) > LOOKER_MASTER_VAR_VALUE_SIZE)
                    return LOOKER_EXIT_NO_MEMORY;
#endif //LOOKER_MASTER_SANITY_TEST
                strcpy(var[msg->payload[1]].value_slave, (const char *) &msg->payload[2]);
            }
            else
                memcpy(var[msg->payload[1]].value_slave, &msg->payload[2], var[msg->payload[1]].size);
            var[msg->payload[1]].value_update = UPDATED_FROM_SLAVE;
        break;

        case RESPONSE_VALUE_NO_MORE:
        break;

        case RESPONSE_STATUS:
            if (msg->payload[1])
            {
                PRINT("Slave rebooted !!!\n");
                master_state_change(MASTER_STATE_RESET);
            }
            slave_state = msg->payload[2];
        break;

        default:
            PRINTLN2("Error: payload_process: wrong response: ", (unsigned char) msg->payload[0]);
            return LOOKER_EXIT_WRONG_RESPONSE;
        break;
    }
    return LOOKER_EXIT_SUCCESS;
}

void looker_destroy(void)
{
#ifdef LOOKER_MASTER_USE_MALLOC
    if (var)
    {
        free(var);
        var = NULL;
    }
#endif //LOOKER_MASTER_USE_MALLOC
}

