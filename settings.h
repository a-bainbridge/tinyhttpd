#ifndef _TINYHTTPD_SETTINGS_H_
#define _TINYHTTPD_SETTINGS_H_
struct settings_s {
    unsigned short port;
    unsigned char verbose;
};

typedef struct settings_s settings_t;

void settings_set_defaults(settings_t*);
settings_t* settings_parse_argv(int,char**);
#endif