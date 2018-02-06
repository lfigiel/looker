#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "looker_stubs.h"
#include "looker.h"
#include "wifi.h"

#define DOMAIN "pc"
#define DEFAULT_STYLE "margin:0;"

//globals
unsigned int i,j = 0;
char k[16] = "empty";

int main(int argc, char *argv[])
{
#include "prefix.c"

    looker_reg("i", &i, sizeof(i), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, DEFAULT_STYLE);
    looker_reg("j", &j, sizeof(j), LOOKER_TYPE_UINT, LOOKER_LABEL_CHECKBOX, DEFAULT_STYLE);
    looker_reg("k", &k, sizeof(k), LOOKER_TYPE_STRING, LOOKER_LABEL_VIEW, DEFAULT_STYLE);

    while (1)
    {
        //index
        i++;

        printf("port: %s, j: %d, k: %s, i: %d\n", argv[1], j, k, i);
        looker_update();

        usleep(100000);
    }

    looker_destroy();
	return 0;
}

