#include <unordered_map>
#include <string>
#include <sstream>

class Response final {
    public:
        std::string status;
        std::unordered_map<std::string, std::string> headers;
        std::string body = "";

        Response(std::string statusLine) {
            status = statusLine;
        }

        Response(std::string statusLine, std::string body) {
            status = statusLine;
            this->body = body;
        }

        std::string to_string() {
            std::stringstream s;
            s << "HTTP/1.1 ";
            s << status << "\r\n";

            for(auto [k,v] : headers) {
                if(k.empty() || v.empty()) continue;
                if(k == "Content-Length" || k == "Connection") continue;

                s << k << ": " << v << "\r\n";
            }
            s << "Content-Length: " << body.length() << "\r\n";
            s << "Connection: keep-alive" << "\r\n";
            s << "\r\n";

            if(!body.empty()) {
                s << body;
            }

            return s.str();
        }
};

#pragma once