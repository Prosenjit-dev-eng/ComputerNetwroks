#include "telnet_server.hpp"
#include <iostream>
#include <csignal>
#include <cstdlib>

static TelnetServer* g_server = nullptr;

static void sigHandler(int) {
    if (g_server) g_server->stop();
}

int main(int argc, char* argv[]) {
    uint16_t port = 23;
    if (argc >= 2) {
        int p = std::atoi(argv[1]);
        if (p <= 0 || p > 65535) {
            std::cerr << "Invalid port: " << argv[1] << "\n";
            return 1;
        }
        port = static_cast<uint16_t>(p);
    }

    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    try {
        TelnetServer server(port);
        g_server = &server;
        std::cout << "Telnet server listening on port " << port << "\n";
        server.run();
        std::cout << "Server stopped.\n";
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
