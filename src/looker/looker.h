/*
Copyright (c) 2018 Lukasz Figiel
*/

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#define LOOKER_USE_MALLOC
#define LOOKER_VAR_COUNT 8
#define LOOKER_VAR_VALUE_SIZE 16
#define LOOKER_VAR_NAME_SIZE 16
#define LOOKER_VAR_STYLE_SIZE 48
#define LOOKER_SANITY_TEST
#define LOOKER_STYLE_DYNAMIC

typedef enum {
    LOOKER_VAR_INT,
    LOOKER_VAR_UINT,
    LOOKER_VAR_FLOAT,
    LOOKER_VAR_STRING,

    LOOKER_VAR_LAST
} LOOKER_VAR_TYPE;

typedef enum {
    LOOKER_HTML_CHECKBOX,
    LOOKER_HTML_CHECKBOX_INV,
    LOOKER_HTML_VIEW,
    LOOKER_HTML_EDIT,

    LOOKER_HTML_LAST
} LOOKER_HTML_TYPE;

typedef enum {
    LOOKER_EXIT_SUCCESS,
    LOOKER_EXIT_NO_MEMORY,
    LOOKER_EXIT_BAD_PARAMETER,
    LOOKER_EXIT_BAD_COMM,
    LOOKER_EXIT_BAD_STATE,
    LOOKER_EXIT_TARGET_TIMEOUT,
    LOOKER_EXIT_SERVER_TIMEOUT,

    LOOKER_EXIT_LAST
} LOOKER_EXIT_CODE;

#ifdef LOOKER_STYLE_DYNAMIC
    #define STYLE_TYPE char *
#else
    #define STYLE_TYPE const char *
#endif //LOOKER_STYLE_DYNAMIC

//prototypes
LOOKER_EXIT_CODE looker_init(const char *ssid, const char *pass, const char *domain);
LOOKER_EXIT_CODE looker_reg(const char *name, volatile void *addr, int size, LOOKER_VAR_TYPE type, LOOKER_HTML_TYPE html, STYLE_TYPE style);
LOOKER_EXIT_CODE looker_update(void);
void looker_destroy(void);
extern int looker_get(void *buf, int size);
extern void looker_send(void *buf, int size);

#ifdef __cplusplus
}
#endif //__cplusplus

