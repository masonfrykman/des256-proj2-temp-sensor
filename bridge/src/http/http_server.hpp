#include <sys/types.h>
#include <string>
#include <unordered_map>
#include <functional>

#include "response.hpp"
#include "request.hpp"

class HTTPServer final {
    private:
        bool _shutdown = false;

        bool _isValidRequestLine(std::string line);
        std::unordered_map<std::string, std::string>* _recievedHead(std::string msg);
        bool _recievedEntireMessage(int sockfd, std::unordered_map<std::string, std::string>* processedHeaders, std::string body);
        void _send(int sockfd, std::string const& message);

        void _recieveRequests(int sockfd);
        void _listenToInterface(struct addrinfo* info, int port);

        

    public:
        HTTPServer() {}

        std::unordered_map<std::string, std::function<Response*(Request&)>> routeHandlers;

        /// @brief Listens for connections on every available IPv4 & IPv6 host using a specified port.
        /// @param port The port to bind to.
        void run(int port);

        void stop();
};

#pragma once