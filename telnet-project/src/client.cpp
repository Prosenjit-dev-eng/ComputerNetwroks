#include "telnet_client.hpp"
#include <iostream>
#include <cstdlib>
#include <csignal>

static TelnetClient* g_client = nullptr;
// This function is an OS signal handler designed to gracefully shut down your Telnet client when the user interrupts or terminates the program (for example, by pressing Ctrl+C in the terminal).
static void sigHandler(int) {
    if (g_client) g_client->stop();
}

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    uint16_t    port = 23;

    if (argc >= 2) host = argv[1];
    if (argc >= 3) {
        int p = std::atoi(argv[2]);
        if (p <= 0 || p > 65535) {
            std::cerr << "Invalid port: " << argv[2] << "\n";
            return 1;
        }
        port = static_cast<uint16_t>(p);
    }

    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    try {
        TelnetClient client(host, port);
        g_client = &client;
        client.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
