#include "net.h"
#include "settings.h"
#define _X86_
#include <winapi/ws2tcpip.h>
#include <winapi/winsock2.h>
#pragma comment(lib, "Ws2_32")
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "http.h"

void net_init() {
    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if(iResult != 0) {
        printf("WSAStartup fail: %d\n", iResult);
        exit(-1);
    }
}

void net_send_data(tinyhttpd_socket client, tinyhttpd_socket server, char* data, size_t data_length) {
    int iResult = send(client, data, data_length, 0);
    if (iResult == SOCKET_ERROR) {
        printf("Send failed: %d\n", WSAGetLastError());
    }
}

tinyhttpd_socket net_create_and_bind_sock(settings_t* settings) {
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
    tinyhttpd_socket sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
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

#define DEFAULT_BUFLEN 1024

void net_handle_connections(settings_t* settings, tinyhttpd_socket server_sock) {
    tinyhttpd_socket ClientSock;
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