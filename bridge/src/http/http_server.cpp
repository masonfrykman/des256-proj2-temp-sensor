#include "http_server.hpp"

#include <stdexcept>
#include <cstring>
#include <string>
#include <thread>
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_set>

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

std::vector<std::string> split(std::string line, char delim) {
    std::vector<std::string> tokens;
    std::stringstream token;
    for(auto iter = line.begin(); iter != line.end(); iter++) {
        char c = *iter;
        if(c == delim) {
            std::string entireToken = token.str();
            if(entireToken.empty()) continue;
            tokens.push_back(entireToken);
        }
    }

    return tokens;
}

// Try to figure out if the data is a valid request line.
// A request line if valid if the following requirements are met:
//  - Splitting by ' ' will result in 3 tokens.
//  - First token is a valid HTTP method (GET, POST, etc.)
//  - Second token begins with /
//  - Third token equals HTTP/1.1
bool HTTPServer::_isValidRequestLine(std::string line) {
    if(line.empty()) return false;

    auto tokens = split(line, ' ');

    if(tokens.size() != 3) {
        return false;
    }

    if(tokens.at(2) != "HTTP/1.1") {
        return false;
    }

    if(*(tokens.at(1).begin()) != '/') {
        return false;
    }

    std::unordered_set<std::string> methods({
        "GET",
        "HEAD",
        "POST",
        "PUT",
        "DELETE",
        "CONNECT",
        "OPTIONS",
        "TRACE",
        "PATCH"
    });

    if(!methods.count(tokens.at(0))) {
        return false;
    }

    return true;
}

std::unordered_map<std::string, std::string>* HTTPServer::_recievedHead(std::string msg) {
    auto lines = split(msg, '\n');

    std::unordered_map<std::string, std::string>* headers = new std::unordered_map<std::string, std::string>();

    // collect headers into the map
    for(size_t i = 1; i < lines.size(); i++) {
        auto headerLine = lines.at(i);
        auto kp = split(headerLine, ':');
        if(kp.size() != 2 || *(kp.at(1).begin()) != ' ' || kp.at(1) == " ") {
            // ignore invalid headers
            continue;
        }
        
        std::string v = kp.at(1);
        v.erase(v.begin()); // trim the ' ' at the beginning.

        (*headers)[kp.at(0)] = v;
    }

    // check headers
    if(headers->size() < 1 || headers->count("Content-Length") == 0) {
        // no valid headers, do not continue.
        (*headers)["__CONTINUE__"] = "false";
        return headers;
    }
    
    // we have at least 1 valid header, now fill in the request line.
    auto rql = split(lines.at(0), ' ');
    (*headers)["__METHOD__"] = rql.at(0);
    (*headers)["__PATH__"] = rql.at(1);

    return headers;
}

void HTTPServer::_recieveRequests(int sockfd) {
    std::stringstream buf; // holds the data for a single request.
    char c; // we recieve data byte by byte.
    
    // mode:
    //  - 0: waiting for request line (ex. GET /index.html HTTP/1.1)
    //      - once the line is recieved, mode -> 1.
    //      - if we're in this mode and we recieve data that doesn't correspond to a valid request line, ignore it.
    //        this allows us to drain data if _recievedHead in mode 1 fails.
    //  - 1: recieving headers. increments on '\n\n' sequence.
    //      - once this has been recieved, call _recievedHead(), which returns a map of the headers + some other useful options.
    //          - if the key "__CONTINUE__" == "true", mode -> 2
    //          - if the key "__CONTINUE__" == "false", mode -> 0
    //  - 2: recieving body. Once the amount specified in Content-Length is reached, call _recievedEntireRequest()
    //      - mode -> 0
    int mode = 0;

    // mode-specific variables.
    bool m1_seenNewline = false;
    std::unordered_map<std::string, std::string>* m1_headers = nullptr;
    int m2_count = 0;
    int m2_contentLength = 0;

    while(!_shutdown) {
        ssize_t r = recv(sockfd, &c, 1, 0);
        if(r == 0) { // connection closed
            break;
        } else if(r == -1) { // error
            // TODO: report error somehow?
            break;
        }

        // Check c depending on the mode.
        switch(mode) {
            case 0: // Awaiting request line
                if(c != '\n') break;

                if(!_isValidRequestLine(buf.str())) {
                    buf = std::stringstream(); // clear the buffer
                    continue;
                }

                // Go to mode 1 on successful request line
                mode++;
                break; // still add the newline to the buffer.

            case 1: // Receiving headers
                if(c == '\n') {
                    if(m1_seenNewline) { // recieved '\n\n'.
                        m1_headers = _recievedHead(buf.str());
                        buf = std::stringstream();
                        
                        if(m1_headers->at("__CONTINUE__") == "false") {
                            mode = 0;
                        } else {
                            mode++;
                            m2_contentLength = std::atoi(m1_headers->at("Content-Length").c_str());
                            m2_count = 0;
                        }

                        m1_seenNewline = false; // reset for next request
                        continue;

                    } else {
                        m1_seenNewline = true;
                    }
                } else {
                    m1_seenNewline = false; // prevent calling _recievedHead after recieving two headers.
                }

                break;

            case 2:
                m2_count++;
                break;
        }

        buf << c;
        if(mode == 2 && m2_count == m2_contentLength) {
            // TODO: call recieved entire body
            mode = 0;
            buf = std::stringstream();
        }
    }

    close(sockfd);
}

void HTTPServer::_listenToInterface(struct addrinfo *info, int port) {
    // TODO: check system calls for errors
    int sockfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);

    int b = bind(sockfd, info->ai_addr, info->ai_addrlen);

    int l = listen(sockfd, 10);

    while(!_shutdown) {
        struct sockaddr remoteAddr;
        socklen_t remoteAddrLen;
        int remoteSockfd = accept(sockfd, &remoteAddr, &remoteAddrLen);

        std::thread t([=](int sockfd) {
            this->_recieveRequests(sockfd);
        }, remoteSockfd);
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
