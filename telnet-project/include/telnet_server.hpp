#pragma once
#include "socket.hpp"
#include "nvt.hpp"
#include "telnet_protocol.hpp"
#include "terminal_session.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>

enum class AuthState {
    AWAITING_USERNAME,
    AWAITING_PASSWORD,
    AUTHENTICATED
};

struct ClientState {
    Socket          sock;
    TelnetProtocol  proto;
    NvtDecoder      nvtDec;
    NvtEncoder      nvtEnc;
    TerminalSession session;
    std::string     inputBuffer;
    bool            alive = true;

    AuthState       authState = AuthState::AWAITING_USERNAME;
    std::string     attemptedUser;
    std::string     authenticatedUser;
    int             failedAttempts = 0;

    explicit ClientState(Socket s) : sock(std::move(s)) {}
};

class TelnetServer {
public:
    explicit TelnetServer(uint16_t port);
    ~TelnetServer();

    void run();
    void stop();

private:
    Socket               listenSock_;
    std::atomic<bool>    running_{false};
    std::vector<std::thread> threads_;
    std::mutex           threadMu_;
    uint16_t             port_;

    void handleClient(Socket sock);
    void sendNegotiation(ClientState& cs);
    void processIncoming(ClientState& cs, const std::vector<uint8_t>& data);
    void sendPrompt(ClientState& cs);
    void sendResponse(ClientState& cs, const std::string& text);
};
