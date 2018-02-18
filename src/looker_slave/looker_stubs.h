#ifndef LOOKER_STUBS_H
#define LOOKER_STUBS_H

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#ifdef LOOKER_STUBS_C
    #define EXTERN
#else
    #define EXTERN extern
#endif //LOOKER_STUBS_C

//debug
//#define DEBUG

#ifdef DEBUG
    #define DEBUG_MSG_DECODE
    #define DEBUG_MSG_ACK
    #define DEBUG_MSG_ACK_WAIT

    EXTERN void debug_print(const char *s);
    EXTERN void debug_println2(const char *s, int i);
    #define PRINT(s) debug_print(s)
    #define PRINTLN2(s,i) debug_println2(s,i)
#else
    #define PRINT(s)
    #define PRINTLN2(s,i)
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

