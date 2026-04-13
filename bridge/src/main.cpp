#include "http/http_server.hpp"

int main(int argc, char* argv[]) {
    HTTPServer s;
    s.run(10100);

    return 0;
}