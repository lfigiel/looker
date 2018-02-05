#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "stubs/looker_stubs.h"
#include "looker.h"
#include "wifi.h"

#define DOMAIN "pc"
#define DEFAULT_STYLE "color:black;font-size:500%;margin:0;"

int main(int argc, char *argv[])
{
#include "prefix.c"

    char text[] = "315,000 MHz";

	if (looker_reg("F", &text, sizeof(text), LOOKER_VAR_STRING, LOOKER_HTML_VIEW, DEFAULT_STYLE))
        printf("Error\n");

    looker_update();
    looker_destroy();
	return 0;
}

