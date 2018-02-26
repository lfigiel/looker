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

    unsigned char var1 = 6;
    unsigned int  var2 = 20;
    unsigned char var3 = 1;
    unsigned char var4 = 1;
    char var5[] = "Hello World";
    int var6 = -100;
    double var7 = 3.141592;

	if ((err = looker_reg("var1", &var1, sizeof(var1), LOOKER_TYPE_UINT, LOOKER_LABEL_VIEW, NULL)))
        printf("Error: %d\n", err);

	if ((err = looker_reg("var2", &var2, sizeof(var2), LOOKER_TYPE_UINT, LOOKER_LABEL_EDIT, NULL)))
        printf("Error: %d\n", err);

	if ((err = looker_reg("var3", &var3, sizeof(var3), LOOKER_TYPE_UINT, LOOKER_LABEL_CHECKBOX, NULL)))
        printf("Error: %d\n", err);

	if ((err = looker_reg("var4", &var4, sizeof(var4), LOOKER_TYPE_UINT, LOOKER_LABEL_CHECKBOX_INV, NULL)))
        printf("Error: %d\n", err);

	if ((err = looker_reg("var5", (void *) var5, strlen(var5), LOOKER_TYPE_STRING, LOOKER_LABEL_VIEW, NULL)))
        printf("Error: %d\n", err);

	if ((err = looker_reg("var6", &var6, sizeof(var6), LOOKER_TYPE_INT, LOOKER_LABEL_VIEW, NULL)))
        printf("Error: %d\n", err);

	if ((err = looker_reg("var7", &var7, sizeof(var7), LOOKER_TYPE_FLOAT_2, LOOKER_LABEL_VIEW, NULL)))
        printf("Error: %d\n", err);

    while (1) {
        looker_update();
        var2++;
    }
    looker_destroy();
	return 0;
}

