#include <stdio.h>
#include <stdlib.h>
#define _X86_
#include <winapi/ws2tcpip.h>
#include <winapi/winsock2.h>
#pragma comment(lib, "Ws2_32")
#include <string.h>

enum {
    HTTP_INVALID_METHOD,
    HTTP_GET,
    HTTP_POST
};

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

    // Resolve the local address and port to be used by the server
    int iResult = getaddrinfo(NULL, "80", &hints, &result);
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
        } else if (strcmp(method, "POST")) {
            r_method = HTTP_POST;
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
    printf("%s\n", request->uri);

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
        response->content_type = "text/html";
    } else if(strstr(uri, ".png")) {
        response->content_type = "image/png";
    } else if (strstr(uri, ".ico")) {
        response->content_type = "image/x-icon";
    } else {
        response->content_type = "text/plain";
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
    void* packet = malloc(response->response_length+128);
    *bytes = sprintf(packet, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", response->response_code, response->response_message, response->content_type, response->response_length-1, response->response_content);
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
        printf("Bytes received: %d\n", iResult);
        http_request_t* request = NULL;
        request = http_parse_request(recvbuf, iResult);
        http_response_t* response;
        if(request == NULL) {
            printf("null request\n");
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

int main(void) {
    printf("httpd\n");
    net_init_winsock();
    SOCKET sock = net_create_and_bind_sock();
    net_handle_connections(sock);
}