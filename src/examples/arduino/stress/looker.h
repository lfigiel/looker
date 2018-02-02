/*
Copyright (c) 2018 Lukasz Figiel
*/

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#define LOOKER_USE_MALLOC
#define LOOKER_VAR_COUNT_MAX 10
#define LOOKER_VAR_VALUE_SIZE 16
#define LOOKER_VAR_NAME_SIZE 16
#define LOOKER_VAR_STYLE_SIZE 48
#define LOOKER_SANITY_TEST
#define LOOKER_STYLE_DYNAMIC

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
    LOOKER_EXIT_TIMEOUT,

    LOOKER_EXIT_LAST
} looker_exit_t;

typedef enum {
    LOOKER_SLAVE_STATE_RESETING,
    LOOKER_SLAVE_STATE_DISCONNECTED,
    LOOKER_SLAVE_STATE_CONNECTING,
    LOOKER_SLAVE_STATE_CONNECTED,

    LOOKER_SLAVE_STATE_LAST
} looker_slave_state_t;

#define LOOKER_VAR_FLOAT LOOKER_VAR_FLOAT_1

#ifdef LOOKER_STYLE_DYNAMIC
    #define STYLE_TYPE char *
#else
    #define STYLE_TYPE const char *
#endif //LOOKER_STYLE_DYNAMIC

//prototypes
looker_exit_t looker_init(const char *ssid, const char *pass, const char *domain);
looker_exit_t looker_reg(const char *name, volatile void *addr, int size, looker_type_t type, looker_label_t label, STYLE_TYPE style);
void looker_update(void);
void looker_destroy(void);
looker_slave_state_t looker_slave_state(void);
#ifdef __cplusplus
}
#endif //__cplusplus

