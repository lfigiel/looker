/*
Copyright (c) 2018 Lukasz Figiel
*/

#ifndef LOOKER_COMMON_H
#define LOOKER_COMMON_H

#define LOOKER_VAR_FLOAT LOOKER_VAR_FLOAT_1

//this is common for master and slave, master stores only pointer to style
#define LOOKER_VAR_STYLE_SIZE (LOOKER_MSG_PAYLOAD_SIZE - 2) //command, var_index

//todo: move to msg.h
#define LOOKER_MSG_SIZE 64 //rx buffer needs to be at least that big
#define LOOKER_MSG_PAYLOAD_SIZE (LOOKER_MSG_SIZE - 3) //msg_prefix, msg_size, msg_checksum
#define LOOKER_MSG_PREFIX (LOOKER_MSG_PAYLOAD_SIZE + 1) //so these two values are different
#define ACK_SUCCESS 0
#define ACK_FAILURE 255

#define LOOKER_STYLE_DISABLED 0
#define LOOKER_STYLE_FIXED 1
#define LOOKER_STYLE_VARIABLE 2

typedef enum {
    LOOKER_TYPE_INT,
    LOOKER_TYPE_UINT,
    LOOKER_TYPE_FLOAT_0,
    LOOKER_TYPE_FLOAT_1,
    LOOKER_TYPE_FLOAT_2,
    LOOKER_TYPE_FLOAT_3,
    LOOKER_TYPE_FLOAT_4,
    LOOKER_TYPE_STRING,

    LOOKER_TYPE_LAST
} looker_type_t;

typedef enum {
    LOOKER_LABEL_SSID,
    LOOKER_LABEL_PASS,
    LOOKER_LABEL_DOMAIN,
    LOOKER_LABEL_CHECKBOX,
    LOOKER_LABEL_CHECKBOX_INV,
    LOOKER_LABEL_VIEW,
    LOOKER_LABEL_EDIT,

    LOOKER_LABEL_LAST
} looker_label_t;

typedef enum {
    LOOKER_EXIT_SUCCESS,
    LOOKER_EXIT_NO_MEMORY,
    LOOKER_EXIT_BAD_PARAMETER,
    LOOKER_EXIT_BAD_COMMAND,
    LOOKER_EXIT_BAD_STATE,
    LOOKER_EXIT_BAD_RESPONSE,
    LOOKER_EXIT_TIMEOUT,
    LOOKER_EXIT_ACK_FAILURE,
    LOOKER_EXIT_NO_DATA,

    LOOKER_EXIT_LAST
} looker_exit_t;

//master also needs it
typedef enum {
    LOOKER_SLAVE_STATE_UNKNOWN,
    LOOKER_SLAVE_STATE_DISCONNECTING,
    LOOKER_SLAVE_STATE_DISCONNECTED,
    LOOKER_SLAVE_STATE_CONNECTING,
    LOOKER_SLAVE_STATE_CONNECTED,

    LOOKER_SLAVE_STATE_LAST
} looker_slave_state_t;

typedef enum {
    LOOKER_NETWORK_DO_NOTHING = 0,
    LOOKER_NETWORK_STAY_CONNECTED,
    LOOKER_NETWORK_STAY_DISCONNECTED,
    LOOKER_NETWORK_RECONNECT,      //phase one - from any state to disconnected
    LOOKER_NETWORK_RECONNECT_2     //phase two - from disconnected to connected
} looker_network_t;

#endif //LOOKER_COMMON_H

