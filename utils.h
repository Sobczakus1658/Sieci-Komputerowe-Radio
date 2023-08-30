#ifndef _UTILS_
#define _UTILS_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>


#include "err.h"

uint16_t read_port(char *string) {
    errno = 0;
    unsigned long port = strtoul(string, NULL, 10);
    PRINT_ERRNO();
    if (port > UINT16_MAX) {
        // fatal("%ul is not a valid port number", port);
        exit(1);
    }
    return (uint16_t) port;
}


struct sockaddr_in get_send_address(char *host, uint16_t port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; 
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    struct addrinfo *address_result;
    CHECK(getaddrinfo(host, NULL, &hints, &address_result));

    struct sockaddr_in send_address;
    send_address.sin_family = AF_INET; 
    send_address.sin_addr.s_addr =
            ((struct sockaddr_in *) (address_result->ai_addr))->sin_addr.s_addr; 
    send_address.sin_port = htons(port); 

    freeaddrinfo(address_result);

    return send_address;
}
bool checkPositiveNumber(char *number) {
    if (number == NULL || *number == '\0') {
        return false;
    }
    if (*number == '-' || *number == '0') {
        return false;
    }
    while (*number != '\0') {
        if (*number < '0' || *number > '9') {
            return false;
        }
        number++;
    }
    return true;
}

bool checkIfExist (int limit, int number){
    if (limit <= number) {
        return false;
    }
    return true;
}

#endif
