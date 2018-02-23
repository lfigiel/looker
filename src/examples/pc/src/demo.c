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

    char imie[20] = "Nie podano";
    unsigned int wiek = 0;
    char *critical_style = "color:red;";
    char style[64];
    strcpy(style, "");

	looker_reg("Imie", imie, 0, LOOKER_TYPE_STRING, LOOKER_LABEL_EDIT, NULL);
	looker_reg("Wiek", &wiek, sizeof(wiek), LOOKER_TYPE_UINT, LOOKER_LABEL_EDIT, NULL);

    do {
        printf("Podaj imie: ");
        fgets (imie, 10, stdin);

        printf("Podaj wiek: ");
        scanf("%d", &wiek);
        getchar();  //needed

        if ((strlen(imie)>0) && (imie[strlen(imie) - 1] == '\n'))
            imie[strlen(imie) - 1] = '\0';

        if (wiek <= 39)
            strcpy(style, "");
        else
            strcpy(style, critical_style);

        looker_update();

        printf("----Imie: %s\n", imie);
        printf("----Wiek: %d\n", wiek);

    } while (wiek > 0);

    looker_destroy();

	return 0;
}

