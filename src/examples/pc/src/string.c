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

    volatile char edit[12] = "123456789";
    char view[12] = "";

	if ((err = looker_reg("edit", edit, sizeof(edit), LOOKER_TYPE_STRING, LOOKER_LABEL_EDIT, NULL)))
        printf("Error: %d\n", err);

	if ((err = looker_reg("view", view, sizeof(view), LOOKER_TYPE_STRING, LOOKER_LABEL_VIEW, NULL)))
        printf("Error: %d\n", err);

    while (1) {
        looker_update();
        strcpy(view, "hello");
    }
    looker_destroy();
	return 0;
}

