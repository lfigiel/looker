#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "looker_stubs.h"
#include "looker_master.h"
#include "wifi.h"

#define DOMAIN "pc"

volatile char edit[12] = "1234567890";
char view[12] = "";

int main(int argc, char *argv[])
{

#include "prefix.c"

	if ((err = looker_reg("edit", edit, 0, LOOKER_TYPE_STRING, LOOKER_LABEL_EDIT, NULL)))
        printf("Error: %d\n", err);

	if ((err = looker_reg("view", view, 0, LOOKER_TYPE_STRING, LOOKER_LABEL_VIEW, NULL)))
        printf("Error: %d\n", err);

    while (1) {
        looker_update();
        strcpy(view, "hello");
    }
    looker_destroy();
	return 0;
}

