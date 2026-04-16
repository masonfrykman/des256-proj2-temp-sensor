#include "http/http_server.hpp"
#include "http/response.hpp"
#include "http/http_server_sec.hpp"
#include "util/env.hpp"

#include <unordered_map>
#include <iostream>
#include <string>
#include <stdexcept>
#include <ctime>
#include <thread>

std::unordered_map<std::string, int> temps;
std::unordered_map<std::string, time_t> times;

int main(int argc, char* argv[]) {
    HTTPServer s;

    s.routeHandlers = {
        {
            "/api/pushTemperature", [](Request& req) -> Response* {
                
                if(req.headers().count("X-Fridge") == 0) {
                    return new Response("401 Unauthenticated");
                }

                if(req.method() == "DELETE") {
                    temps.erase(req.headers().at("X-Fridge"));
                    return new Response("200 OK");
                }

                if(req.method() != "POST") {
                    return new Response("404 Not Found");
                }

                std::string temp = req.body();
                int conv;
                try {
                    conv = std::atoi(temp.c_str());
                } catch(std::exception e) {
                    return new Response("400 Bad Request");
                }

                temps[req.headers().at("X-Fridge")] = conv;
                times[req.headers().at("X-Fridge")] = time(NULL);
                return new Response("200 OK");
            }
        },
        {
            "/api/all", [](Request& req) -> Response* {
                std::stringstream col;
                
                for(auto [k,v] : temps) {
                    col << k << " " << std::to_string(v) << " " << std::to_string(times.at(k)) << std::endl;
                }

                Response* res = new Response("200 OK");
                res->body = col.str();
                return res;
            }
        },
        {
            "/api/temp", [](Request& req) -> Response* {
                if(req.headers().count("X-Fridge") == 0) {
                    return new Response("401 Unauthorized");
                }

                if(temps.count(req.headers().at("X-Fridge")) == 0) {
                    return new Response("404 Not Found");
                }

                Response* res = new Response("200 OK");
                res->body = "" + std::to_string(temps.at(req.headers().at("X-Fridge"))) + " " + std::to_string(times.at(req.headers().at("X-Fridge"))) + "\n";
                return res;
            }
        }
    };

    

    std::thread* ss = new std::thread([s](){
        std::string pk = sgetenv("BRIDGE_PK");
        std::string certChain = sgetenv("BRIDGE_CERTCHAIN");
        if(pk.empty() || certChain.empty()) {
            std::cout << "BRIDGE_PK and BRIDGE_CERTCHAIN environment variables not set, not starting HTTPS server." << std::endl;
            return;
        }

        SecureHTTPServer secureServer(certChain, pk);

        secureServer.routeHandlers = s.routeHandlers;

        secureServer.run(443);
    });
    ss->detach();
    delete ss;
    

    s.run(80);

    return 0;
}