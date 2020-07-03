#include <stdlib.h>
#pragma once
#ifndef _TINYHTTPD_HTTP_H_
#define _TINYHTTPD_HTTP_H_
#define METHOD_MAX_LENGTH 16
#define URI_MAX_LENGTH 255
#define VERSION_MAX_LENGTH 8
#define RESPONSE_MAX_LENGTH 65535

enum {
    HTTP_INVALID_METHOD,
    HTTP_GET
};

struct http_request_s {
    unsigned char method;
    char uri[URI_MAX_LENGTH];
};

typedef struct http_request_s http_request_t;

struct http_response_s {
    int response_code;
    char* response_message;
    char* content_type;
    int response_length;
    char* response_content;
};

typedef struct http_response_s http_response_t;

http_request_t* http_parse_request(char*, size_t);
http_response_t* http_generate_bad_request_response();
http_response_t* http_generate_not_found_response();
http_response_t* http_generate_normal_response(http_request_t*);
void* http_encode_response(http_response_t*, int*);
#endif