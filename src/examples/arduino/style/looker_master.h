/*
Copyright (c) 2018 Lukasz Figiel
*/

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "looker_common.h"

#define LOOKER_MASTER_USE_MALLOC
#define LOOKER_MASTER_VAR_COUNT 16
#define LOOKER_MASTER_VAR_NAME_SIZE (LOOKER_MSG_PAYLOAD_SIZE - 5) //command, var_index, var_size, var_type, var_label
#define LOOKER_MASTER_VAR_VALUE_SIZE 16
#define LOOKER_MASTER_STYLE LOOKER_STYLE_VARIABLE
#define LOOKER_MASTER_SANITY_TEST

#if LOOKER_MASTER_STYLE == LOOKER_STYLE_VARIABLE
    #define looker_style_t const char **
#else
    #define looker_style_t const char *
#endif //LOOKER_MASTER_STYLE == LOOKER_STYLE_VARIABLE

//prototypes
void looker_init(void);
looker_exit_t looker_connect(const char *ssid, const char *pass, const char *domain);
void looker_disconnect(void);
looker_exit_t looker_reg(const char *name, volatile void *addr, int size, looker_type_t type, looker_label_t label, looker_style_t style);
looker_exit_t looker_update(void);
void looker_destroy(void);
looker_slave_state_t looker_slave_state(void);
#ifdef __cplusplus
}
#endif //__cplusplus

