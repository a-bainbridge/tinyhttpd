#include "settings.h"
#include <stdlib.h>
#ifndef _TINYHTTPD_NET_H_
#define _TINYHTTP_NET_H_
typedef unsigned int tinyhttpd_socket;
void net_init();
tinyhttpd_socket net_create_and_bind_sock(settings_t*);
void net_handle_connections(settings_t*, tinyhttpd_socket);
void net_send_data(tinyhttpd_socket, tinyhttpd_socket, char*, size_t);
#endif