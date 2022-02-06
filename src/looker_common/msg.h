/*
Copyright (c) 2018 Lukasz Figiel
*/

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "looker_common.h"

#define MSG_TIMEOUT 2000    /*ms*/

typedef enum {
    COMMAND_RESET,
    COMMAND_CONNECT,
    COMMAND_DISCONNECT,
    COMMAND_STATUS_GET,
    COMMAND_VAR_REG,
    COMMAND_VALUE_SET,
    COMMAND_STYLE_SET,
    COMMAND_VALUE_GET,
    COMMAND_VALUE_REPEAT,

    COMMAND_LAST,
    RESPONSE_ACK_SUCCESS,
    RESPONSE_ACK_FAILURE,
    RESPONSE_STATUS,
    RESPONSE_VALUE,
    RESPONSE_VALUE_NO_MORE,

    RESPONSE_LAST
} control_t;

typedef enum {
    //receive
    POS_PREFIX,
    POS_PAYLOAD_SIZE,
    POS_PAYLOAD,
    POS_CHECKSUM,
    //send
    POS_ACK,

    POS_LAST
} msg_pos_t;

typedef struct {
//prefix omitted because is fixed
    unsigned char payload_size;
    unsigned char payload[LOOKER_MSG_PAYLOAD_SIZE];
    unsigned char crc;
} msg_t;

//prototypes
looker_exit_t ack_get(unsigned char *ack);
void ack_send(unsigned char ack);
void msg_begin(msg_t *msg, unsigned char control);
looker_exit_t msg_get(msg_t *msg);
void msg_send(msg_t *msg);
void msg_decode(msg_t *msg, unsigned char in);
#ifdef __cplusplus
}
#endif //__cplusplus

