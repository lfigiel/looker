#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "stubs/looker_stubs.h"
#include "looker.h"
#include "wifi.h"

#define DOMAIN "pc"
#define DEFAULT_STYLE "margin:0;"

int main(int argc, char *argv[])
{
#include "prefix.c"

    unsigned int t = 0;

    float f = 3.14;
    double d = 4.14;

	if (looker_reg("f", &f, sizeof(f), LOOKER_VAR_FLOAT, LOOKER_HTML_EDIT, DEFAULT_STYLE))
        printf("Error\n");

	if (looker_reg("d", &d, sizeof(d), LOOKER_VAR_FLOAT, LOOKER_HTML_EDIT, DEFAULT_STYLE))
        printf("Error\n");

    printf("%ld\n", sizeof(f));
    printf("%ld\n", sizeof(d));

    while (1)
    {
        printf("t: %d, f: %.4f, d: %.4f\n", t++, f, d);

        looker_update();

        usleep(300000);
    }

    looker_destroy();
	return 0;
}

