#include "http_server_sec.hpp"

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

static std::vector<std::string> split(std::string line, char delim) {
    std::vector<std::string> tokens;
    std::stringstream token;
    for(auto iter = line.begin(); iter != line.end(); iter++) {
        char c = *iter;
        if(c == delim) {
            std::string entireToken = token.str();
            if(entireToken.empty()) continue;
            tokens.push_back(entireToken);
            token = std::stringstream();
            continue;
        }
        token << c;
    }

    std::string trailingToken = token.str();
    if(!trailingToken.empty()) {
        tokens.push_back(trailingToken);
    }

    return tokens;
}

// Try to figure out if the data is a valid request line.
// A request line if valid if the following requirements are met:
//  - Splitting by ' ' will result in 3 tokens.
//  - First token is a valid HTTP method (GET, POST, etc.)
//  - Second token begins with /
//  - Third token equals HTTP/1.1
bool SecureHTTPServer::_isValidRequestLine(std::string line) {
    if(line.empty()) return false;

    auto tokens = split(line, ' ');

    if(tokens.size() != 3) {
        return false;
    }

    if(tokens.at(2) != "HTTP/1.1\r") {
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

std::unordered_map<std::string, std::string>* SecureHTTPServer::_recievedHead(std::string msg) {
    auto lines = split(msg, '\n');

    std::unordered_map<std::string, std::string>* headers = new std::unordered_map<std::string, std::string>();

    // collect headers into the map
    for(size_t i = 1; i < lines.size(); i++) {
        auto headerLine = lines.at(i);
        auto sep = headerLine.find_first_of(": ");
        if(sep == std::string::npos || sep + 2 >= headerLine.length()) {
            continue;
        }

        auto k = headerLine.substr(0, sep);
        auto v = headerLine.substr(sep + 2);
        
        headers->emplace(k, v);
    }

    // check headers
    if(headers->size() < 2) {
        // no valid headers, do not continue.
        (*headers)["__CONTINUE__"] = "false";
        return headers;
    }
    
    // we have at least 1 valid header, now fill in the request line.
    auto rql = split(lines.at(0), ' ');
    (*headers)["__METHOD__"] = rql.at(0);
    (*headers)["__PATH__"] = rql.at(1);
    (*headers)["__CONTINUE__"] = "true";

    return headers;
}

bool SecureHTTPServer::_recievedEntireMessage(SSL* sockfd, std::unordered_map<std::string, std::string>* processedHeaders, std::string body) {
    if(processedHeaders->count("__PATH__") == 0) {
        // invalid header object
        delete processedHeaders;
        return false;
    }

    std::string path = (*processedHeaders)["__PATH__"];
    
    Request req(processedHeaders->at("__METHOD__"), processedHeaders->at("__PATH__"), (*processedHeaders), body);
    
    // try to send the request to a handler
    if(routeHandlers.count(path) == 0) {
        // return a 404 not found response
        Response res("404 Not Found");
        res.body = "'" + path + "' not found.";

        _send(sockfd, res.to_string());
        return true;
    }
    
    Response* res = routeHandlers[path](req);
    if(res == nullptr) { // returning null means close the connection immediately.
        delete processedHeaders;
        return false;
    }

    _send(sockfd, res->to_string());

    delete res;
    delete processedHeaders;
    return true;
}

void SecureHTTPServer::_send(SSL* sock, std::string const& message) {
    if(message.empty()) return;

    std::string b = message;
    while(!b.empty()) {
        //ssize_t s = send(sockfd, b.c_str(), b.length(), 0);
        auto s = SSL_write(sock, b.c_str(), b.length());
        if(s == -1) {
            throw std::runtime_error("Send failed, errno=" + std::to_string(errno));
        }

        b = b.substr(s);
    }
}

void SecureHTTPServer::_recieveRequests(int sockfd, SSL_CTX* ctx) {
    // Setup SSL
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);

    if(SSL_accept(ssl) <= 0) {
        // TODO: err
        return;
    }

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
    int m1_seqCR = 0;
    int m1_seqLF = 0;
    bool m1_seenNewline = false;
    std::unordered_map<std::string, std::string>* m1_headers = nullptr;
    int m2_count = 0;
    int m2_contentLength = 0;

    while(!_shutdown) {
        //ssize_t r = recv(sockfd, &c, 1, 0);
        auto r = SSL_read(ssl, &c, 1);
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
                if(c == '\n' || c == '\r') {
                    if(c == '\n' && m1_seqCR == 2 && m1_seqLF == 1) { // recieved '\r\n\r\n'.
                        m1_headers = _recievedHead(buf.str());
                        buf = std::stringstream();
                        
                        if(m1_headers->at("__CONTINUE__") == "false") {
                            mode = 0;
                        } else {
                            mode++;
                            if(m1_headers->count("Content-Length") != 0) {
                                m2_contentLength = std::atoi(m1_headers->at("Content-Length").c_str());
                            } else {
                                bool x = _recievedEntireMessage(ssl, m1_headers, buf.str()); // note: deletes m1_headers
                                if(!x) {
                                    SSL_free(ssl);
                                    return;
                                }
                                mode = 0;
                            }
                            m2_count = 0;
                        }

                        m1_seenNewline = false; // reset for next request
                        continue;

                    } else {
                        if(c == '\n') {
                            m1_seqLF++;
                        } else {
                            m1_seqCR++;
                        }
                    }
                } else {
                    m1_seqCR = 0;
                    m1_seqLF = 0;
                }

                if(c == '\r') {
                    continue; // clean the headers output for carriage returns.
                }

                break;

            case 2:
                m2_count++;
                break;
        }

        buf << c;
        if(mode == 2 && m2_count == m2_contentLength) { // mode == 3 -> empty body
            bool x = _recievedEntireMessage(ssl, m1_headers, buf.str()); // note: deletes m1_headers
            if(!x) {
                break; // stop recieving.
            }
            mode = 0;
            buf = std::stringstream();
        }
    }

    SSL_free(ssl);
    close(sockfd);
}

void SecureHTTPServer::_listenToInterface(struct addrinfo *info, int port) {
    // TODO: check system calls for errors
    int sockfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);

    int b = bind(sockfd, info->ai_addr, info->ai_addrlen);
    if(b == -1) {
        throw std::runtime_error("bind failed! errno=" + std::to_string(errno));
    }

    int l = listen(sockfd, 10);
    if(l == -1) {
        throw std::runtime_error("bind failed! errno=" + std::to_string(errno));
    }

    // ssl
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = TLS_server_method();
    ctx = SSL_CTX_new(method);
    if (ctx == NULL) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        return;
    }

    if (SSL_CTX_use_certificate_chain_file(ctx, "/etc/letsencrypt/live/t1.frykman.dev/fullchain.pem") <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, "/etc/letsencrypt/live/t1.frykman.dev/privkey.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    while(!_shutdown) {
        struct sockaddr remoteAddr;
        socklen_t remoteAddrLen;
        int remoteSockfd = accept(sockfd, &remoteAddr, &remoteAddrLen);

        std::thread* t = new std::thread([=](int sockfd) {
            this->_recieveRequests(sockfd, ctx);
        }, remoteSockfd);
    }

    delete info;
}

void SecureHTTPServer::run(int port) {
    if(port < 1 || port > 65535) {
        throw std::runtime_error("Port must be in range 1-65535.");
    }

    _shutdown = false;

    // Get all valid interfaces to listen on
    struct addrinfo hints;
    struct addrinfo *servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int lookupStatus = getaddrinfo(NULL, std::to_string(port).c_str(), &hints, &servinfo);
    if(lookupStatus != 0) {
        throw std::runtime_error("getaddrinfo failed!");
    }

    // walk through the given interfaces & begin listening to them
    struct addrinfo* curr = servinfo;
    while(curr != NULL) {
        std::thread* t = new std::thread([=] (struct addrinfo* info, int port) {
            this->_listenToInterface(info, port);
        }, curr, port);

        curr = curr->ai_next;
    }

    while(!_shutdown) {}
}

void SecureHTTPServer::stop() {
    // Causes _listenToInterface to stop accepting connections.
    _shutdown = true;
}
