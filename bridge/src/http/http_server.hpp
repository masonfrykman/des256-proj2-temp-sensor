
#include <sys/types.h>

class HTTPServer final {
    private:
        bool _shutdown = false;

        void _listenToInterface(struct addrinfo* info, int port);

    public:
        HTTPServer() {}

        /// @brief Listens for connections on every available IPv4 & IPv6 host using a specified port.
        /// @param port The port to bind to.
        void run(int port);

        void stop();
};

#pragma once