#include <string>
#include <unordered_map>

class Request final {
    private:
        std::string _method;
        std::string _path;
        std::unordered_map<std::string, std::string> _headers;
        std::string _body;

    public:
        Request(std::string method, std::string path, std::unordered_map<std::string, std::string>& headers, std::string body) {
            _method = method;
            _path = path;
            _headers = headers;
            _body = body;
        }

        std::string const& body() {
            return _body;
        }

        std::string const& path() {
            return _path;
        }

        std::string const& method() {
            return _method;
        }

        std::unordered_map<std::string, std::string> const& headers() {
            return _headers;
        }
};

#pragma once