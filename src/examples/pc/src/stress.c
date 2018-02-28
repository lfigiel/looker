#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "looker_stubs.h"
#include "looker_master.h"
#include "wifi.h"

#define DOMAIN "pc"
#define DEFAULT_STYLE "margin:0;"

#define STAT(fn) stat_update(fn)

//globals
volatile unsigned long i,j;
volatile char k[16] = "empty";

volatile unsigned char looker_debug;
unsigned char looker_debug_old;

volatile size_t stat_success;
volatile size_t stat_no_memory;
volatile size_t stat_wrong_param;
volatile size_t stat_wrong_comm;
volatile size_t stat_wrong_state;
volatile size_t stat_timeout;
volatile size_t stat_ack_failure;
volatile size_t stat_no_data;

const char *style = DEFAULT_STYLE;

void stat_update(looker_exit_t err)
{
    switch (err) {
        case LOOKER_EXIT_SUCCESS:
            stat_success++;
        break;
        case LOOKER_EXIT_NO_MEMORY:
            stat_no_memory++;
        break;
        case LOOKER_EXIT_WRONG_PARAMETER:
            stat_wrong_param++;
        break;
        case LOOKER_EXIT_WRONG_COMMAND:
            stat_wrong_comm++;
        break;
        case LOOKER_EXIT_WRONG_STATE:
            stat_wrong_state++;
        break;
        case LOOKER_EXIT_TIMEOUT:
            stat_timeout++;
        break;
        case LOOKER_EXIT_ACK_FAILURE:
            stat_ack_failure++;
        break;
        case LOOKER_EXIT_NO_DATA:
            stat_no_data++;
        break;
        default:
        break;
    }
}

int main(int argc, char *argv[])
{
#include "prefix.c"

    STAT(looker_reg("i", &i, sizeof(i), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, &style));
    STAT(looker_reg("j", &j, sizeof(j), LOOKER_TYPE_UINT, LOOKER_LABEL_CHECKBOX, &style));
    STAT(looker_reg("k", &k, 0, LOOKER_TYPE_STRING, LOOKER_LABEL_VIEW, &style));

    STAT(looker_reg("looker_debug", &looker_debug, sizeof(looker_debug), LOOKER_TYPE_UINT, LOOKER_LABEL_CHECKBOX, &style));

    STAT(looker_reg("success", &stat_success, sizeof(stat_success), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, &style));
    STAT(looker_reg("no_memory", &stat_no_memory, sizeof(stat_no_memory), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, &style));
    STAT(looker_reg("wrong_param", &stat_wrong_param, sizeof(stat_wrong_param), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, &style));
    STAT(looker_reg("wrong_comm", &stat_wrong_comm, sizeof(stat_wrong_comm), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, &style));
    STAT(looker_reg("wrong_state", &stat_wrong_state, sizeof(stat_wrong_state), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, &style));
    STAT(looker_reg("timeout", &stat_timeout, sizeof(stat_timeout), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, &style));
    STAT(looker_reg("ack_failure", &stat_ack_failure, sizeof(stat_ack_failure), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, &style));
    STAT(looker_reg("no_data", &stat_no_data, sizeof(stat_no_data), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, &style));

    while (1)
    {
        //index
        i++;

        if ((!looker_debug) && (looker_debug_old))
        {
            stat_success = 0;
            stat_no_memory = 0;
            stat_wrong_param = 0;
            stat_wrong_comm = 0;
            stat_wrong_state = 0;
            stat_timeout = 0;
            stat_ack_failure = 0;
            stat_no_data = 0;

            looker_debug_old = 0;
        }

        if (looker_debug)
            looker_debug_old = 1;

        STAT(looker_update());
//        sleep(1);
    }

    looker_destroy();
	return 0;
}

