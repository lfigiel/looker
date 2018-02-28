#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "looker_stubs.h"
#include "looker_master.h"
#include "wifi.h"

#define DOMAIN "pc"
#define DEFAULT_STYLE "color:red;font-size:500%;margin:0;"

int main(int argc, char *argv[])
{
#include "prefix.c"

    char text[] = "315,000 MHz";
    const char *style = DEFAULT_STYLE;

	if (looker_reg("F", &text, 0, LOOKER_TYPE_STRING, LOOKER_LABEL_VIEW, &style))
        printf("Error\n");

    while (1) {
        looker_update();
        sleep(1);
    }
    looker_destroy();
	return 0;
}

