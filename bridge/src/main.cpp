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

std::string psk;

int main(int argc, char* argv[]) {
    HTTPServer s;

    // Load the pre-shared secret in from the environment
    psk = sgetenv("BRIDGE_PSK");
    if(psk.empty()) {
        std::cerr << "FATAL ERROR: BRIDGE_PSK must be set." << std::endl;
        return 1;
    }

    // Expose some routes to for the public API.
    s.routeHandlers = {
        {
            // Sets the temperature of a fridge, timestamping with the time it was recieved.
            //  - POST: sets a temperature. Body should be a single integer representing the temperature of the fridge.
            //  - DELETE: Removes any reference to the fridge.
            
            "/api/report", [](Request& req) -> Response* {
                if(req.headers().count("X-PSK") == 0) {
                    return new Response("401 Unauthorized", "X-PSK must be set.");
                }

                if(req.headers().at("X-PSK") != psk) {
                    return new Response("403 Forbidden", "bad PSK.");
                }
                
                if(req.headers().count("X-Fridge") == 0) {
                    return new Response("400 Bad Request", "No fridge specified.");
                }

                if(req.method() == "DELETE") {
                    temps.erase(req.headers().at("X-Fridge"));
                    times.erase(req.headers().at("X-Fridge"));
                    return new Response("200 OK", "Fridge information deleted.");
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
                return new Response("200 OK", "Successfully set temperature.");
            }
        },
        {
            // Gets all the temperatures & times stored.
            // Responds in a space-seperated list:
            // <fridge name> <temp> <unix time set>
            // ex.
            // LoveFridge 36 1776357010
            // LoveShack 39 1776291012
            "/api/temp/all", [](Request& req) -> Response* {
                if(temps.size() < 1) {
                    return new Response("404 Not Found", "No fridge temperatures have been set yet.");
                }

                std::stringstream col;
                
                for(auto [k,v] : temps) {
                    col << k << "|" << std::to_string(v) << "|" << std::to_string(times.at(k)) << std::endl;
                }

                return new Response("200 OK", col.str());
            }
        },
        {
            // Get the temperature & time set of a single fridge.
            "/api/temp", [](Request& req) -> Response* {
                if(req.method() != "GET") {
                    return new Response("404 Not Found", "'" + req.path() + "' not found.");
                }

                if(req.headers().count("X-Fridge") == 0) {
                    return new Response("400 Bad Request", "X-Fridge header must be specified.");
                }

                if(temps.count(req.headers().at("X-Fridge")) == 0) {
                    return new Response("404 Not Found", "Invalid fridge name.");
                }

                Response* res = new Response("200 OK");
                res->body = "" + std::to_string(temps.at(req.headers().at("X-Fridge"))) + " " + std::to_string(times.at(req.headers().at("X-Fridge"))) + "\n";
                return res;
            }
        }
    };

    // Start the secure server in another thread.
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
    
    // Start the HTTP server in this thread (blocks)
    s.run(80);

    return 0;
}