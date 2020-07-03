#include "http.h"
#include "net.h"
#include "settings.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char* RESPONSE_OK = "OK";
char* RESPONSE_BAD_REQUEST = "Bad Request";
char* RESPONSE_NOT_FOUND = "Not Found";
char* BAD_REQUEST_DEFAULT = "400 Bad Request\r\n";
char* NOT_FOUND_REQUEST_DEFAULT = "404 Not Found\r\n";
char* CONTENT_TYPE_HTTP = "text/html";

http_request_t* http_parse_request(char* buffer, size_t buflen) {
    if(buflen < 8) {
        return NULL;
    }
    http_request_t* request = malloc(sizeof(http_request_t));
    char* bp = buffer;
    for(;bp-buffer < METHOD_MAX_LENGTH;bp++) {
        if(*bp == ' ') {
            break;
        }
        if(bp-buffer > buflen) {
            free(request);
            return NULL;
        }
    }
    if(bp-buffer > buflen) {
        free(request);
        return NULL;
    }
    unsigned char r_method = HTTP_INVALID_METHOD;
    if(bp-buffer < METHOD_MAX_LENGTH) {
        char method[METHOD_MAX_LENGTH] = {};
        sscanf(buffer, "%s", &method);
        if(strcmp(method, "GET")) {
            r_method = HTTP_GET;
        }
        
    }
    bp++;
    char* uri_start = bp;
    request->method = r_method;
    for(;bp-buffer < METHOD_MAX_LENGTH+1+URI_MAX_LENGTH;bp++) {
        if(*bp == ' ') {
            break;
        }
        if(bp-buffer > buflen) {
            free(request);
            return NULL;
        }
    }
    
    char r_uri[URI_MAX_LENGTH] = {'b','a','d','\0'};
    if(bp-buffer < METHOD_MAX_LENGTH+1+URI_MAX_LENGTH) {
        sscanf(uri_start, "%s", r_uri);
    }
    bp++;
    memset(request->uri, 0, URI_MAX_LENGTH);
    memcpy(&request->uri, r_uri, URI_MAX_LENGTH);
    request->uri[URI_MAX_LENGTH-1] = '\0';

    return request;
}

http_response_t* http_generate_bad_request_response() {
    http_response_t* response = malloc(sizeof(http_response_t));
    response->response_code = 400;
    response->response_message = RESPONSE_BAD_REQUEST;
    response->response_content = BAD_REQUEST_DEFAULT;
    response->content_type = CONTENT_TYPE_HTTP;
    response->response_length = 18;
    return response;
}
http_response_t* http_generate_not_found_response() {
    http_response_t* response = malloc(sizeof(http_response_t));
    response->response_code = 404;
    response->response_message = RESPONSE_NOT_FOUND;
    response->response_content = NOT_FOUND_REQUEST_DEFAULT;
    response->content_type = CONTENT_TYPE_HTTP;
    response->response_length = 16;
    return response;
}

http_response_t* http_generate_normal_response(http_request_t* request) {
    if(strstr(request->uri, "..") != NULL) {
        return http_generate_bad_request_response();
    }
    if(strstr(request->uri, "//") != NULL) {
        return http_generate_bad_request_response();
    }
    if(strstr(request->uri, "\\") != NULL) {
        return http_generate_bad_request_response();
    }
    char* uri = request->uri;
    if(strcmp(request->uri, "/") == 0) {
        uri = "index.html";
    }
    char dest[URI_MAX_LENGTH + 7] = "site";
    if(uri[0] != '/') {
        dest[4] = '/';
        dest[5] = '\0';
    }
    strncat(dest, uri, URI_MAX_LENGTH);
    FILE* file = fopen(dest, "rb");
    if(file == NULL) {
        return http_generate_not_found_response();
    }
    fseek(file, 0, SEEK_END);
    int file_length = ftell(file);
    fseek(file, 0, SEEK_SET);
    http_response_t* response = malloc(sizeof(http_response_t));
    if(strstr(uri, ".html") || strstr(uri, ".htm")) {
        response->content_type = "text/html; charset=utf-8";
    } else if(strstr(uri, ".png")) {
        response->content_type = "image/png";
    } else if (strstr(uri, ".ico")) {
        response->content_type = "image/x-icon";
    } else {
        response->content_type = "text/plain; charset=utf-8";
    }
    char* content = malloc(file_length+1);
    content[file_length] = '\0';
    fread(content, file_length, 1, file);
    fclose(file);
    response->response_length = file_length+1;
    response->response_content = content;
    response->response_code = 200;
    response->response_message = RESPONSE_OK;
    return response;
}

void* http_encode_response(http_response_t* response, int* bytes) {
    void* packet = malloc(response->response_length+256);
    *bytes += sprintf_s(packet, 256, "HTTP/1.1 %d %s\r\nServer: tinyhttpd\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
                      response->response_code, response->response_message, response->content_type, response->response_length-1);
    memcpy((packet+*bytes), response->response_content, response->response_length);
    *bytes += response->response_length;
    return packet;
}