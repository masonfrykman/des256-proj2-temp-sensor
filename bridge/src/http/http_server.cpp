#include "http_server.hpp"

#include <stdexcept>
#include <cstring>
#include <string>
#include <thread>
#include <iostream>

#include <sys/socket.h>
#include <netdb.h>

void HTTPServer::_listenToInterface(struct addrinfo* info, int port) {
    // TODO: check system calls for errors
    int sockfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);

    int b = bind(sockfd, info->ai_addr, info->ai_addrlen);

    int l = listen(sockfd, 10);

    while(!_shutdown) {
        struct sockaddr remoteAddr;
        socklen_t remoteAddrLen;
        int remoteSockfd = accept(sockfd, &remoteAddr, &remoteAddrLen);

        // TODO: begin listening for requests on the individual socket
    }
}

void HTTPServer::run(int port) {
    if(port < 1 || port > 65535) {
        throw std::runtime_error("Port must be in range 1-65535.");
    }

    _shutdown = false;

    // Get all valid interfaces to listen on
    struct addrinfo hints;
    struct addrinfo *servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int lookupStatus = getaddrinfo(NULL, std::to_string(port).c_str(), &hints, &servinfo);
    if(lookupStatus != 0) {
        throw std::runtime_error("getaddrinfo failed!");
    }

    // walk through the given interfaces & begin listening to them
    struct addrinfo* curr = servinfo;
    while(curr != NULL) {
        std::thread t([=] (struct addrinfo* info, int port) {
            struct addrinfo* ninfo = (struct addrinfo *)malloc(sizeof(struct addrinfo));
            memcpy(ninfo, info, sizeof(struct addrinfo));
            this->_listenToInterface(ninfo, port); // most of the thread's lifetime is spent in here
            free(ninfo);
        }, curr, port);

        curr = curr->ai_next;
    }

    freeaddrinfo(servinfo);
}

void HTTPServer::stop() {
    // Causes _listenToInterface to stop accepting connections.
    _shutdown = true;
}
