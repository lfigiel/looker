#include <stdio.h>
#include <stdlib.h>
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

    unsigned char play = 0;

	if (looker_reg("Play", &play, sizeof(play), LOOKER_VAR_UINT, LOOKER_HTML_CHECKBOX, DEFAULT_STYLE))
        printf("Error\n");

    int i = 0;

    while (1)
    {
        printf("%d play: %d\n", ++i, play);

        looker_update();

        if (play)
        {
            system("mplayer bowie.mp3");
            play = 0;
        }

        sleep(1);
    }

    looker_destroy();
	return 0;
}

