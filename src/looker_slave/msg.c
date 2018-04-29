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
                    PRINT("LOOKER_TYPE_INT\n");
                break;
                case LOOKER_TYPE_UINT:
                    PRINT("LOOKER_TYPE_UINT\n");
                break;
                case LOOKER_TYPE_FLOAT_0:
                    PRINT("LOOKER_TYPE_FLOAT_0\n");
                break;
                case LOOKER_TYPE_FLOAT_1:
                    PRINT("LOOKER_TYPE_FLOAT_1\n");
                break;
                case LOOKER_TYPE_FLOAT_2:
                    PRINT("LOOKER_TYPE_FLOAT_2\n");
                break;
                case LOOKER_TYPE_FLOAT_3:
                    PRINT("LOOKER_TYPE_FLOAT_3\n");
                break;
                case LOOKER_TYPE_FLOAT_4:
                    PRINT("LOOKER_TYPE_FLOAT_4\n");
                break;
                case LOOKER_TYPE_STRING:
                    PRINT("LOOKER_TYPE_STRING\n");
                break;
                default:
                    PRINT("Unknown !!!\n");
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
                    PRINT("LOOKER_LABEL_SSID\n");
                break;
                case LOOKER_LABEL_PASS:
                    PRINT("LOOKER_LABEL_PASS\n");
                break;
                case LOOKER_LABEL_DOMAIN:
                    PRINT("LOOKER_LABEL_DOMAIN\n");
                break;
                case LOOKER_LABEL_CHECKBOX:
                    PRINT("LOOKER_LABEL_CHECKBOX\n");
                break;
                case LOOKER_LABEL_CHECKBOX_INV:
                    PRINT("LOOKER_LABEL_CHECKBOX_INV\n");
                break;
                case LOOKER_LABEL_VIEW:
                    PRINT("LOOKER_LABEL_VIEW\n");
                break;
                case LOOKER_LABEL_EDIT:
                    PRINT("LOOKER_LABEL_EDIT\n");
                break;
                default:
                    PRINT("Unknown !!!\n");
                break;
            }
        }
        break;

        case COMMAND_VALUE_SET:
            PRINT(dir);PRINT("COMMAND_VALUE_SET\n");
            //var index
            PRINTLN2("  var_index: ", msg->payload[1]);
        break;

        case COMMAND_STYLE_SET:
            PRINT(dir);PRINT("COMMAND_STYLE_SET\n");
            //var index
            PRINTLN2("  var_index: ", msg->payload[1]);
            //style
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

        case RESPONSE_ACK_SUCCESS:
            PRINT(dir);PRINT("RESPONSE_ACK_SUCCESS\n");
        break;

        case RESPONSE_ACK_FAILURE:
            PRINT(dir);PRINT("RESPONSE_ACK_FAILURE\n");
        break;

        case RESPONSE_STATUS:
            PRINT(dir);PRINT("RESPONSE_STATUS\n");
            PRINT("  slave_state: ");
            switch (msg->payload[2]) {
                case LOOKER_SLAVE_STATE_UNKNOWN:
                    PRINT("LOOKER_SLAVE_STATE_UNKNOWN\n");
                break;
                case LOOKER_SLAVE_STATE_DISCONNECTING:
                    PRINT("LOOKER_SLAVE_STATE_DISCONNECTING\n");
                break;
                case LOOKER_SLAVE_STATE_DISCONNECTED:
                    PRINT("LOOKER_SLAVE_STATE_DISCONNECTED\n");
                break;
                case LOOKER_SLAVE_STATE_CONNECTING:
                    PRINT("LOOKER_SLAVE_STATE_CONNECTING\n");
                break;
                case LOOKER_SLAVE_STATE_CONNECTED:
                    PRINT("LOOKER_SLAVE_STATE_CONNECTED\n");
                break;
                default:
                    PRINT("UNDEFINED !!!\n");
                break;
            }
        break;

        case RESPONSE_VALUE:
            PRINT(dir);PRINT("RESPONSE_VALUE\n");
            //var index
            PRINTLN2("  var_index: ", msg->payload[1]);
        break;

        case RESPONSE_VALUE_NO_MORE:
            PRINT(dir);PRINT("RESPONSE_VALUE_NO_MORE\n");
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

looker_exit_t ack_get(unsigned char *ack)
{
    msg_t msg;
    looker_exit_t err;

    if ((err = msg_get(&msg)) == LOOKER_EXIT_SUCCESS)
    {
        if ((msg.payload[0] == RESPONSE_ACK_FAILURE) || (msg.payload[0] == RESPONSE_ACK_SUCCESS))
        {
            *ack = msg.payload[0];
#ifndef LOOKER_COMBO
            if (msg.payload[0] == RESPONSE_ACK_FAILURE)
                stat_ack_get_failures++;
#endif //LOOKER_COMBO
            return LOOKER_EXIT_SUCCESS;
        }
    }
    return err;
}

void ack_send(unsigned char ack)
{
    msg_t msg;
    msg_begin(&msg, ack);
    msg_send(&msg);

#ifndef LOOKER_COMBO
    if (ack == RESPONSE_ACK_FAILURE)
        stat_ack_send_failures++;
#endif //LOOKER_COMBO
}

looker_exit_t msg_get(msg_t *msg)
{
    unsigned char rx;
    msg_pos_t msg_pos = POS_PREFIX;
    unsigned int pay_pos;
    unsigned int j;

    j = 0;
    do {
        looker_delay_1ms();

        if (looker_data_available())
        {
            do {
                looker_get(&rx, 1);
                switch (msg_pos) {
                    case POS_PREFIX:
                        //check prefix
                        if (rx != LOOKER_MSG_PREFIX)
                        {
                            PRINTLN2("Error: msg_get: prefix: ", rx);
                            return LOOKER_EXIT_WRONG_PREFIX;
                        }
                        else
                            msg_pos = POS_PAYLOAD_SIZE; 
                    break;
                    case POS_PAYLOAD_SIZE:
                        //check payload size
                        if (rx > LOOKER_MSG_PAYLOAD_SIZE)
                        {
                            PRINTLN2("Error: msg_get: payload size: ", rx);
                            return LOOKER_EXIT_WRONG_PAYLOAD_SIZE;
                        }
                        else
                        {
                            msg->payload_size = rx;
#ifdef DEBUG_MSG_CHECKSUM
                            PRINTLN2("msg: rx (payload size): ", msg->payload_size);
#endif //DEBUG_MSG_CHECKSUM
                            msg->crc = crc_8(&rx, 1);
#ifdef DEBUG_MSG_CHECKSUM
                            PRINTLN2("msg: crc: ", msg->crc);
#endif //DEBUG_MSG_CHECKSUM
                            pay_pos = 0;
                            msg_pos = POS_PAYLOAD; 
                        }
                    break;

                    case POS_PAYLOAD:
                        msg->payload[pay_pos] = rx;
#ifdef DEBUG_MSG_CHECKSUM
                        PRINTLN2("msg: rx: ", msg->payload[pay_pos]);
#endif //DEBUG_MSG_CHECKSUM
                        msg->crc = update_crc_8(msg->crc, rx);
#ifdef DEBUG_MSG_CHECKSUM
                        PRINTLN2("msg: crc: ", msg->crc);
#endif //DEBUG_MSG_CHECKSUM
                        if (++pay_pos >= msg->payload_size)
                            msg_pos = POS_CHECKSUM; 
                    break;

                    case POS_CHECKSUM:
                        //check checksum
                        if (msg->crc == rx)
                        {
#ifdef DEBUG_MSG_DELAY
                            PRINTLN2("  msg_get: delay: ", j);
#endif //DEBUG_MSG_WAIT
#ifdef DEBUG_MSG_DECODE
                            msg_decode(msg, 1);
#endif //DEBUG_MSG_DECODE
                            return LOOKER_EXIT_SUCCESS;
                        }
                        else
                        {
                            PRINTLN2("Error: msg_get: checksum: ", rx);
                            return LOOKER_EXIT_WRONG_CHECKSUM;
                        }
                    break;

                    default:
                        PRINT("Error: msg_get: pos\n");
                        return LOOKER_EXIT_WRONG_DATA;
                    break;
                }
            } while (looker_data_available());
        }

    } while (++j < MSG_TIMEOUT);

    //timeout
    if (j >= MSG_TIMEOUT)
    {
        PRINTLN2("Error: msg_get: timeout: ", j);
        return LOOKER_EXIT_TIMEOUT;
    }

    return LOOKER_EXIT_SUCCESS;
}

void msg_send(msg_t *msg)
{
    unsigned char rx, tx, crc;

    //empty rx buffer
    while (looker_data_available())
        looker_get(&rx, 1);

    //send prefix
    tx = LOOKER_MSG_PREFIX;
    looker_send(&tx, 1);
#ifdef DEBUG_MSG_CHECKSUM
    PRINTLN2("msg: tx: ", tx);
#endif //DEBUG_MSG_CHECKSUM

    //send payload size
    looker_send(&msg->payload_size, 1);
#ifdef DEBUG_MSG_CHECKSUM
    PRINTLN2("msg: tx (payload size): ", msg->payload_size);
#endif //DEBUG_MSG_CHECKSUM
    crc = crc_8(&msg->payload_size, 1);
#ifdef DEBUG_MSG_CHECKSUM
    PRINTLN2("msg: crc: ", crc);
#endif //DEBUG_MSG_CHECKSUM

    //send payload
    size_t i;
    for (i=0; i<msg->payload_size; i++)
    {
        looker_send(&msg->payload[i], 1);
#ifdef DEBUG_MSG_CHECKSUM
        PRINTLN2("msg: tx: ", msg->payload[i]);
#endif //DEBUG_MSG_CHECKSUM
        crc = update_crc_8(crc, msg->payload[i]);
#ifdef DEBUG_MSG_CHECKSUM
        PRINTLN2("msg: crc: ", crc);
#endif //DEBUG_MSG_CHECKSUM
    }

    //send checksum
    looker_send(&crc, sizeof(crc));
#ifdef DEBUG_MSG_CHECKSUM
    PRINTLN2("msg: tx: ", crc);
#endif //DEBUG_MSG_CHECKSUM

#ifdef DEBUG_MSG_DECODE
    msg_decode(msg, 0);
#endif //DEBUG_MSG_DECODE
}

