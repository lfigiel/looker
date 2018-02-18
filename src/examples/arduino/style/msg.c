/*
Copyright (c) 2018 Lukasz Figiel
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "msg.h"
#include "checksum.h"
#include "looker_stubs.h"

#ifndef LOOKER_COMBO
    size_t stat_ack_get_failures;
    size_t stat_ack_send_failures;
#endif //LOOKER_COMBO

void msg_decode(msg_t *msg, unsigned char in)
{
    const char *dir;
    if (in)
    {
        dir = "<-";
    }
    else
    {
        dir = "->";
    }

    switch (msg->payload[0]) {
        case COMMAND_RESET:
            PRINT(dir);PRINT("COMMAND_RESET\n");
        break;

        case COMMAND_CONNECT:
            PRINT(dir);PRINT("COMMAND_CONNECT\n");
        break;

        case COMMAND_DISCONNECT:
            PRINT(dir);PRINT("COMMAND_DISCONNECT\n");
        break;

        case COMMAND_STATUS_GET:
            PRINT(dir);PRINT("COMMAND_STATUS_GET\n");
        break;

        case COMMAND_VAR_REG:
        {
            PRINT(dir);PRINT("COMMAND_VAR_REG\n");

            size_t i = 1;

            //var index
            PRINTLN2("  var_index: ", msg->payload[i++]);

            //size
            PRINTLN2("  size: ", msg->payload[i++]);

            //type
            PRINT("  type: ");
            switch (msg->payload[i++]) {
                case LOOKER_TYPE_INT:
                    PRINT("int\n");
                break;
                case LOOKER_TYPE_UINT:
                    PRINT("uint\n");
                break;
                case LOOKER_TYPE_FLOAT_0:
                    PRINT("float_0\n");
                break;
                case LOOKER_TYPE_FLOAT_1:
                    PRINT("float_1\n");
                break;
                case LOOKER_TYPE_FLOAT_2:
                    PRINT("float_2\n");
                break;
                case LOOKER_TYPE_FLOAT_3:
                    PRINT("float_3\n");
                break;
                case LOOKER_TYPE_FLOAT_4:
                    PRINT("float_4\n");
                break;
                case LOOKER_TYPE_STRING:
                    PRINT("string\n");
                break;
                default:
                    PRINT("unknown !!!\n");
                break;
            }

            //name
            PRINT("  name: ");
            if (strlen((char *) &msg->payload[i]))
            {
                PRINT((char *) &msg->payload[i]);PRINT("\n");
            }
            else
            {
                PRINT("NULL\n");
            }
            i += strlen((char *) &msg->payload[i]) + 1;

            //label
            PRINT("  label: ");
            switch (msg->payload[i++]) {
                case LOOKER_LABEL_SSID:
                    PRINT("ssid\n");
                break;
                case LOOKER_LABEL_PASS:
                    PRINT("pass\n");
                break;
                case LOOKER_LABEL_DOMAIN:
                    PRINT("domain\n");
                break;
                case LOOKER_LABEL_CHECKBOX:
                    PRINT("checkbox\n");
                break;
                case LOOKER_LABEL_CHECKBOX_INV:
                    PRINT("checkbox_inv\n");
                break;
                case LOOKER_LABEL_VIEW:
                    PRINT("view\n");
                break;
                case LOOKER_LABEL_EDIT:
                    PRINT("edit\n");
                break;
                default:
                    PRINT("unknown !!!\n");
                break;
            }
        }
        break;

        case COMMAND_VALUE_SET:
            PRINT(dir);PRINT("COMMAND_VALUE_SET\n");
        break;

        case COMMAND_STYLE_SET:
            PRINT(dir);PRINT("COMMAND_STYLE_SET\n");
            PRINT("  style: ");
            if (msg->payload[2])
            {
                PRINT((char *) &msg->payload[2]); PRINT("\n");
            }
            else
                PRINT("NULL\n");
        break;

        case COMMAND_VALUE_GET:
            PRINT(dir);PRINT("COMMAND_VALUE_GET\n");
        break;

        case RESPONSE_STATUS:
            PRINT(dir);PRINT("RESPONSE_STATUS\n");
        break;

        case RESPONSE_VALUE:
            PRINT(dir);PRINT("RESPONSE_VALUE\n");
        break;

        case RESPONSE_VALUE_LAST:
            PRINT(dir);PRINT("RESPONSE_VALUE_LAST\n");
        break;

        default:
            PRINT("Error: msg_decode: control undefined\n");
        break;
    }
}

void msg_begin(msg_t *msg, unsigned char control)
{
    msg->payload_size = 0;
    msg->payload[msg->payload_size++] = control;
}

unsigned char ack_get(void)
{
    uint8_t ack;
    looker_get(&ack, 1);

    if (ack != ACK_SUCCESS)
    {
        PRINT("<-ACK_FAILURE\n");
#ifndef LOOKER_COMBO
        stat_ack_get_failures++;
#endif //LOOKER_COMBO
    }
#ifdef DEBUG_MSG_ACK
    else
        PRINT("<-ACK_SUCCESS\n");
#endif //DEBUG_MSG_ACK
    return ack;
}

void ack_send(msg_t *msg, unsigned char ack)
{
    unsigned char rx;

    //reset msg position
    msg->pos = POS_PREFIX;

    //empty rx buffer
    while (looker_data_available())
        looker_get(&rx, 1);

    looker_send(&ack, 1);

    if (ack != ACK_SUCCESS)
    {
        PRINT("->ACK_FAILURE\n");
#ifndef LOOKER_COMBO
        stat_ack_send_failures++;
#endif //LOOKER_COMBO
    }
#ifdef DEBUG_MSG_ACK
    else
        PRINT("->ACK_SUCCESS\n");
#endif //DEBUG_MSG_ACK
}

unsigned char msg_get(msg_t *msg)
{
    unsigned char rx;

    while (looker_data_available())
    {
        looker_get(&rx, 1);
        switch (msg->pos) {
            case POS_PREFIX:
                msg->complete = 0;
                //check prefix
                if (rx != LOOKER_MSG_PREFIX)
                {
//todo: remove msg->pos = POS_PREFIX everywhere in this function, this is reset elsewhere
                    msg->pos = POS_PREFIX;
                    PRINT("Error: msg_get: prefix\n");
                    return 1;
                }
                else
                    msg->pos++; 
            break;
            case POS_PAYLOAD_SIZE:
                //check payload size
                if (rx > LOOKER_MSG_PAYLOAD_SIZE)
                {
                    msg->pos = POS_PREFIX;
                    PRINT("Error: msg_get: payload size\n");
                    return 2;
                }
                else
                {
                    msg->payload_size = rx;
                    msg->payload_pos = 0;
                    msg->crc = crc_8(&rx, 1);
//PRINTF1("msg: crc: %u\n", msg->crc);
                    msg->pos++; 
                }
            break;

            case POS_PAYLOAD:
                msg->payload[msg->payload_pos] = rx;
//PRINTF1("msg: rx: %u\n", msg->payload[msg->payload_pos]);
                msg->crc = update_crc_8(msg->crc, rx);
//PRINTF1("msg: crc: %u\n", msg->crc);
                if (++msg->payload_pos >= msg->payload_size)
                    msg->pos++; 
            break;

            case POS_CHECKSUM:
                //check checksum
                if (msg->crc == rx)
                {
                    msg->pos = POS_PREFIX;
                    msg->complete = 1;
                }
                else
                {
                    msg->pos = POS_PREFIX;
                    PRINT("Error: msg_get: checksum\n");
                    return 3;
                }
            break;

            default:
                msg->pos = POS_PREFIX;
                PRINT("Error: msg_get: pos\n");
                return 4;
            break;
        }
    }
    return LOOKER_EXIT_SUCCESS;
}

void msg_send(msg_t *msg)
{
    unsigned char rx, tx, crc;

    //reset msg position
    msg->pos = POS_PREFIX;

    //empty rx buffer
    while (looker_data_available())
        looker_get(&rx, 1);

    //send prefix
    tx = LOOKER_MSG_PREFIX;
    looker_send(&tx, 1);
//PRINTF1("msg: tx: %u\n", tx);

    //send payload size
    looker_send(&msg->payload_size, 1);
//PRINTF1("msg: tx: %u\n", msg->payload_size);
    crc = crc_8(&msg->payload_size, 1);
//PRINTF1("msg: crc: %u\n", crc);

    //send payload
    size_t t;
    for (t=0; t<msg->payload_size; t++)
    {
        looker_send(&msg->payload[t], 1);
//PRINTF1("msg: tx: %u\n", msg->payload[t]);
        crc = update_crc_8(crc, msg->payload[t]);
//PRINTF1("msg: crc: %u\n", crc);
    }

    //send checksum
    looker_send(&crc, sizeof(crc));
//PRINTF1("msg: tx: %u\n", crc);

#ifdef DEBUG_MSG_DECODE
    msg_decode(msg, 0);
#endif //DEBUG_MSG_DECODE
}

unsigned char msg_complete(msg_t *msg)
{
    unsigned char complete = msg->complete;
    msg->complete = 0;
    return complete;
}

looker_exit_t ack_wait(unsigned char *ack)
{
    //wait for ack
    size_t j;
    for (j=0; j<ACK_TIMEOUT; j++)
    {
        looker_delay_1ms();
        if (looker_data_available())
        {
            *ack = ack_get();
            break;
        }
    }

    //timeout
    if (j >= ACK_TIMEOUT)
    {
        PRINTLN2("  ack_wait: timeout: ", j);
        return LOOKER_EXIT_TIMEOUT;
    }
#ifdef DEBUG_MSG_ACK_WAIT
        else
            PRINTLN2("  ack_wait: ", j);
#endif //DEBUG_MSG_ACK_WAIT
    return LOOKER_EXIT_SUCCESS;
}

