#include "settings.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
void settings_set_defaults(settings_t* settings) {
    settings->port = 80;
    settings->verbose = 0;
}

void settings_print_help() {
    printf("tinyhttpd help\n");
    printf("-h    --help        View command line arguments and usage.\n");
    printf("-v    --verbose     Enable additional logging.\n");
    printf("--port=[port]       Set port to listen on. Default: 80.\n");
    exit(1);
}

unsigned short settings_parse_port(char* argument) {
    int length = strlen(argument);
    if(length < 8) {
        return 0;
    }
    argument += 7;
    unsigned short port;
    if(!sscanf(argument, "%hu", &port)) {
        return 0;
    } else {
        return port;
    }
}

settings_t* settings_parse_argv(int argc, char* argv[]) {
    settings_t* settings = malloc(sizeof(settings_t));
    settings_set_defaults(settings);
    for(;argc > 1;argc--) {
        if(strcmp("-h",argv[argc-1]) == 0 || strcmp("--help",argv[argc-1]) == 0) {
            settings_print_help();
        } else if(strcmp("-v",argv[argc-1]) == 0 || strcmp("--verbose",argv[argc-1]) == 0)  {
            settings->verbose = 1;
        } else if(strstr(argv[argc-1],"--port=")) {
            unsigned short port = settings_parse_port(argv[argc-1]);
            if(!port) {
                printf("Bad command line option %s.\n", argv[argc-1]);
                settings_print_help();
            }
            settings->port = port;
        } else {
            printf("Unknown command line option %s.\n", argv[argc-1]);
            settings_print_help();
        }
    }
    return settings;
}