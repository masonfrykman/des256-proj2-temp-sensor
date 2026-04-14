
#include <sys/types.h>
#include <string>
#include <unordered_map>

class HTTPServer final {
    private:
        bool _shutdown = false;

        bool _isValidRequestLine(std::string line);
        std::unordered_map<std::string, std::string>* _recievedHead(std::string msg);

        void _recieveRequests(int sockfd);
        void _listenToInterface(struct addrinfo* info, int port);

    public:
        HTTPServer() {}

        /// @brief Listens for connections on every available IPv4 & IPv6 host using a specified port.
        /// @param port The port to bind to.
        void run(int port);

        void stop();
};

#pragma once