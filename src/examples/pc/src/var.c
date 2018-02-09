#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "stubs/looker_stubs.h"
#include "looker_master.h"
#include "wifi.h"

#define DOMAIN "pc"
#define DEFAULT_STYLE "margin:0;"

int main(int argc, char *argv[])
{
#include "prefix.c"

    char imie[30] = "Nie podano";
    unsigned int wiek = 0;

	if (looker_reg("Imie", imie, 0, LOOKER_VAR_STRING, LOOKER_HTML_EDIT, DEFAULT_STYLE))
        printf("Error\n");
	if (looker_reg("Wiek", &wiek, sizeof(wiek), LOOKER_VAR_UINT, LOOKER_HTML_EDIT, DEFAULT_STYLE))
        printf("Error\n");

    int i = 0;

    while (1)
    {
        printf("%d imie: %s, wiek: %d\n", ++i, imie, wiek);

        looker_update();

        usleep(200000);
    }

    looker_destroy();
	return 0;
}

