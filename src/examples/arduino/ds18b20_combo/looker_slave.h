/*
Copyright (c) 2018 Lukasz Figiel
*/

#include "looker_common.h"

//these are valid also in combo mode for both master and slave
#define LOOKER_SLAVE_USE_MALLOC
#define LOOKER_SLAVE_VAR_COUNT 64
#define LOOKER_SLAVE_VAR_NAME_SIZE (LOOKER_MSG_PAYLOAD_SIZE - 5) //command, var_index, var_size, var_type, var_label
#define LOOKER_SLAVE_VAR_VALUE_SIZE (LOOKER_MSG_PAYLOAD_SIZE - 2) //command, var_index
#define LOOKER_SLAVE_STYLE LOOKER_STYLE_VARIABLE
#define LOOKER_SLAVE_SANITY_TEST

#if LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE
    #define looker_style_t const char **
#else
    #define looker_style_t const char *
#endif //LOOKER_SLAVE_STYLE == LOOKER_STYLE_VARIABLE

