#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "looker_stubs.h"
#include "looker_master.h"
#include "wifi.h"

#define DOMAIN "pc"
#define DEFAULT_STYLE "margin:0;"

int main(int argc, char *argv[])
{
#include "prefix.c"

    unsigned int t = 0;

//    long long          i =  4000000000000000000LL;
    long long          i =  -9223372036854775808LU;
//    unsigned long long u = 18000000000000000000LU;
    unsigned long long u = 18446744073709551615LU;

	if (looker_reg("i", &i, sizeof(i), LOOKER_TYPE_INT, LOOKER_LABEL_EDIT, DEFAULT_STYLE))
        printf("Error\n");

	if (looker_reg("u", &u, sizeof(u), LOOKER_TYPE_UINT, LOOKER_LABEL_EDIT, DEFAULT_STYLE))
        printf("Error\n");

    while (1)
    {
        printf("t: %d, i: %lld, u: %llu\n", t++, i, u);

        looker_update();

        usleep(300000);
    }

    looker_destroy();
	return 0;
}

