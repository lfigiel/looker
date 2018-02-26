#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "looker_stubs.h"
#include "looker_master.h"
#include "wifi.h"

#define DOMAIN "pc"

int main(int argc, char *argv[])
{
#include "prefix.c"

    char imie[30] = "Nie podano";
    unsigned int wiek = 0;

	if (looker_reg("Imie", imie, 0, LOOKER_TYPE_STRING, LOOKER_LABEL_EDIT, NULL))
        printf("Error\n");
	if (looker_reg("Wiek", &wiek, sizeof(wiek), LOOKER_TYPE_UINT, LOOKER_LABEL_EDIT, NULL))
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

