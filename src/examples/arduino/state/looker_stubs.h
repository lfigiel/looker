#ifndef LOOKER_STUBS_H
#define LOOKER_STUBS_H

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

//#ifndef LOOKER_STUBS_C
//    #define EXTERN extern
//#else
    #define EXTERN
//#endif //LOOKER_STUBS_C

//debug
//#define DEBUG

#ifdef DEBUG
    #define PRINTF(a) Serial.print(a)
    #define PRINTF1(a,b) Serial.print(a); Serial.println(b,DEC)
    #define PRINTF2(a,b,c)
#else
    #define PRINTF(a)
    #define PRINTF1(a,b)
    #define PRINTF2(a,b,c)
#endif //DEBUG

//prototypes
EXTERN void looker_delay_1ms(void);
EXTERN size_t looker_data_available(void);
EXTERN int looker_get(void *buf, int size);
EXTERN void looker_send(void *buf, int size);
EXTERN int serial_init(const char *portname);
#ifdef __cplusplus
}
#endif //__cplusplus

#endif //LOOKER_STUBS_H

