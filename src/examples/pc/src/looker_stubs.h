#ifndef LOOKER_STUBS_H
#define LOOKER_STUBS_H

#ifndef LOOKER_STUBS_C
    #define EXTERN extern
#else
    #define EXTERN
#endif //LOOKER_STUBS_C

//debug
#define DEBUG

#ifdef DEBUG
    #define PRINTF(a) printf(a)
    #define PRINTF1(a,b) printf(a,b)
    #define PRINTF2(a,b,c) printf(a,b,c)
#else
    #define PRINTF(a)
    #define PRINTF1(a,b)
    #define PRINTF2(a,b,c)
#endif //DEBUG

//prototypes
EXTERN void looker_delay(void);
EXTERN size_t looker_data_available(void);
EXTERN int looker_get(void *buf, int size);
EXTERN void looker_send(void *buf, int size);
EXTERN int serial_init(const char *portname);

#endif //LOOKER_STUBS_H

