#include "looker_master.h"
#include "looker_stubs.h"
#include "wifi.h"

#define LOOKER_DOMAIN "arduino"

volatile uint8_t u8_ = 0;
volatile int8_t s8_ = 0;
volatile uint16_t u16_ = 0;
volatile int16_t s16_ = 0;
volatile uint32_t u32_ = 0;
volatile int32_t s32_ = 0;
volatile uint64_t u64_ = 0;
volatile int64_t s64_ = 0;
volatile float f = 0.0;
volatile double d = 0.0;

void setup() {
    looker_stubs_init(NULL);
    looker_wifi_connect(LOOKER_SSID, LOOKER_PASS, LOOKER_DOMAIN);

    looker_reg("u8", &u8_, sizeof(u8_), LOOKER_TYPE_UINT, LOOKER_LABEL_EDIT, NULL);
    looker_reg("s8", &s8_, sizeof(s8_), LOOKER_TYPE_INT, LOOKER_LABEL_EDIT, NULL);
    looker_reg("u16", &u16_, sizeof(u16_), LOOKER_TYPE_UINT, LOOKER_LABEL_EDIT, NULL);
    looker_reg("s16", &s16_, sizeof(s16_), LOOKER_TYPE_INT, LOOKER_LABEL_EDIT, NULL);
    looker_reg("u32", &u32_, sizeof(u32_), LOOKER_TYPE_UINT, LOOKER_LABEL_EDIT, NULL);
    looker_reg("s32", &s32_, sizeof(s32_), LOOKER_TYPE_INT, LOOKER_LABEL_EDIT, NULL);
    looker_reg("u64", &u64_, sizeof(u64_), LOOKER_TYPE_UINT, LOOKER_LABEL_EDIT, NULL);
    looker_reg("s64", &s64_, sizeof(s64_), LOOKER_TYPE_INT, LOOKER_LABEL_EDIT, NULL);
    looker_reg("f", &f, sizeof(f), LOOKER_TYPE_FLOAT, LOOKER_LABEL_EDIT, NULL);
    looker_reg("d", &d, sizeof(d), LOOKER_TYPE_FLOAT, LOOKER_LABEL_EDIT, NULL);
}

void loop() {
    u8_ += 1;
    s8_ -= 1;
    u16_ += 1;
    s16_ -= 1;
    u32_ += 1;
    s32_ -= 1;
    u64_ += 1;
    s64_ -= 1;
    f += .1;
    d += .2;

    looker_update();
}
