#include <stdio.h>
#include <stdlib.h>
#define _X86_
#include <winapi/ws2tcpip.h>
#include <winapi/winsock2.h>
#pragma comment(lib, "Ws2_32")
#include <string.h>

#define METHOD_MAX_LENGTH 16
#define URI_MAX_LENGTH 255
#define VERSION_MAX_LENGTH 8
#define RESPONSE_MAX_LENGTH 65535

char* RESPONSE_OK = "OK";
char* RESPONSE_BAD_REQUEST = "Bad Request";
char* RESPONSE_NOT_FOUND = "Not Found";
char* BAD_REQUEST_DEFAULT = "400 Bad Request\r\n";
char* NOT_FOUND_REQUEST_DEFAULT = "404 Not Found\r\n";
char* CONTENT_TYPE_HTTP = "text/html";

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

struct settings_s {
    unsigned short port;
    unsigned char verbose;
};

typedef struct settings_s settings_t;

settings_t* settings;

void net_init_winsock() {
    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if(iResult != 0) {
        printf("WSAStartup fail: %d\n", iResult);
        exit(-1);
    }
}

void net_send_data(SOCKET client, SOCKET server, char* data, size_t data_length) {
    int iResult = send(client, data, data_length, 0);
    if (iResult == SOCKET_ERROR) {
        printf("Send failed: %d\n", WSAGetLastError());
    }
}

SOCKET net_create_and_bind_sock() {
    struct addrinfo *result = NULL, *ptr = NULL, hints = {};

    ZeroMemory(&hints, sizeof (hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    char port_to_string_buf[6] = {};
    sprintf_s(port_to_string_buf, 6, "%d", settings->port);
    if(settings->verbose) {
        printf("Creating socket on port %s\n", port_to_string_buf);
    }

    // Resolve the local address and port to be used by the server
    int iResult = getaddrinfo(NULL, port_to_string_buf, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed: %d\n", iResult);
        WSACleanup();
        exit(1);
    }
    SOCKET sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == INVALID_SOCKET) {
        printf("Error creating socket: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        exit(1);
    }
    iResult = bind(sock, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("Error binding socket: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        exit(1);
    }
    freeaddrinfo(result);
    iResult = listen(sock, SOMAXCONN);
    if(iResult == SOCKET_ERROR) {
        printf("Error listening: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        exit(1);
    }
    return sock;
}



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


#define DEFAULT_BUFLEN 1024

void net_handle_connections(SOCKET server_sock) {
    SOCKET ClientSock;
    char addr_string[16] = {};
    char recvbuf[DEFAULT_BUFLEN] = {};
    int recvbuflen = DEFAULT_BUFLEN;
    struct sockaddr_in client_data;
    int size = sizeof(client_data);
    while(1) {
        ZeroMemory(&client_data, sizeof(client_data));
        size = sizeof(client_data);
        ClientSock = INVALID_SOCKET;
        ClientSock = accept(server_sock, &client_data, &size);
        if (ClientSock == INVALID_SOCKET) {
            printf("Accept failed: %d\n", WSAGetLastError());
            closesocket(server_sock);
            exit(1);
        }
        int len = 16;
        WSAAddressToStringA(&client_data, size, NULL, addr_string, (LPDWORD)&len);
        printf("Connection from %s\n", addr_string);
        int iResult = recv(ClientSock, recvbuf, recvbuflen, 0);
        http_request_t* request = NULL;
        request = http_parse_request(recvbuf, iResult);
        http_response_t* response;
        if(request == NULL) {
            response = http_generate_bad_request_response();
        } else {
            response = http_generate_normal_response(request);
        }
        if (response != NULL) {
            int length = 0;
            void* resp = http_encode_response(response, &length);
            net_send_data(ClientSock, server_sock, resp, length);
            free(resp);
            if(response->response_code == 200) {
                free(response->response_content);
            }
            free(response);
        }
        if(request != NULL) {
            free(request);
        }
        closesocket(ClientSock);
    }
}

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

int main(int argc, char *argv[]) {
    settings = settings_parse_argv(argc, argv);
    printf("tinyhttpd 1.0\n");
    if(settings->verbose) {
        printf("Verbose logging enabled.\n");
    }
    net_init_winsock();
    SOCKET sock = net_create_and_bind_sock();
    if(settings->verbose) {
        printf("Ready to accept connections.\n");
    }
    net_handle_connections(sock);
}