#pragma once
#include "socket.hpp"
#include "nvt.hpp"
#include "telnet_protocol.hpp"
#include <string>
#include <thread>
#include <atomic>

class TelnetClient {
public:
    TelnetClient(const std::string& host, uint16_t port);
    ~TelnetClient();

    void run();
    void stop();

private:
    std::string       host_;
    uint16_t          port_;
    Socket            sock_;
    TelnetProtocol    proto_;
    NvtEncoder        nvtEnc_;
    NvtDecoder        nvtDec_;
    std::atomic<bool> running_{false};
    std::thread       recvThread_;
    std::atomic<bool> isPasswordMode_{false};
    std::atomic<bool> localEchoOff_{false};   // set by IAC WONT ECHO from server

    void sendNegotiation();
    void recvLoop();
    void sendLoop();
    std::string readLine(std::atomic<bool>& echoOff);
};
