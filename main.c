#include "settings.h"
#include "net.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char *argv[]) {
    settings_t* settings = settings_parse_argv(argc, argv);
    printf("tinyhttpd 1.0\n");
    if(settings->verbose) {
        printf("Verbose logging enabled.\n");
    }
    net_init();
    tinyhttpd_socket sock = net_create_and_bind_sock(settings);
    if(settings->verbose) {
        printf("Ready to accept connections.\n");
    }
    net_handle_connections(settings, sock);
}