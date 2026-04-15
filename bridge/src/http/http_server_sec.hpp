#include <sys/types.h>
#include <string>
#include <unordered_map>
#include <functional>

#include "response.hpp"
#include "request.hpp"

#include <openssl/ssl.h>
#include <openssl/err.h>

class SecureHTTPServer {
    private:
        bool _shutdown = false;

        bool _isValidRequestLine(std::string line);
        std::unordered_map<std::string, std::string>* _recievedHead(std::string msg);
        bool _recievedEntireMessage(SSL* sockfd, std::unordered_map<std::string, std::string>* processedHeaders, std::string body);
        void _send(SSL* sock, std::string const& message);

        void _recieveRequests(int sockfd, SSL_CTX* ctx);
        void _listenToInterface(struct addrinfo* info, int port);

        

    public:
        SecureHTTPServer() {}

        std::unordered_map<std::string, std::function<Response*(Request&)>> routeHandlers;

        /// @brief Listens for connections on every available IPv4 & IPv6 host using a specified port.
        /// @param port The port to bind to.
        void run(int port);

        void stop();
};

#pragma once