/*
Copyright (c) 2018 Lukasz Figiel
*/

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "looker_common.h"

#define ACK_TIMEOUT 1000    //ms

typedef enum {
    COMMAND_RESET,
    COMMAND_CONNECT,
    COMMAND_DISCONNECT,
    COMMAND_STATUS_GET,
    COMMAND_VAR_REG,
    COMMAND_VALUE_SET,
    COMMAND_STYLE_SET,
    COMMAND_VALUE_GET,

    COMMAND_LAST,
    RESPONSE_STATUS,
    RESPONSE_VALUE,
    RESPONSE_VALUE_LAST,

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
} pos_t;

typedef struct {
//prefix omitted because is fixed
    unsigned char pos;
    unsigned char payload_size;
    unsigned char payload[LOOKER_MSG_PAYLOAD_SIZE];
    unsigned char payload_pos;
    unsigned char crc;
    unsigned char complete;
} msg_t;

//prototypes
unsigned char ack_get(void);
void ack_send(msg_t *msg, unsigned char ack);
void msg_begin(msg_t *msg, unsigned char control);
unsigned char msg_complete(msg_t *msg);
unsigned char msg_get(msg_t *msg);
void msg_send(msg_t *msg);
looker_exit_t ack_wait(unsigned char *ack);
void msg_decode(msg_t *msg, unsigned char in);
#ifdef __cplusplus
}
#endif //__cplusplus

