#include "telnet_server.hpp"
#include <iostream>
#include <cstring>
#include <cerrno>
#include <cctype>
#include <vector>
#include <string>

#ifndef _WIN32
#include <sys/select.h>
#endif

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
TelnetServer::TelnetServer(uint16_t port)
    : listenSock_(Socket::createTCP()), port_(port)
{
    listenSock_.setReuseAddr();
    listenSock_.bind("0.0.0.0", port);
    listenSock_.listen(10);
}

TelnetServer::~TelnetServer() {
    stop();
}

// ---------------------------------------------------------------------------
// run — accept loop
// ---------------------------------------------------------------------------
void TelnetServer::run() {
    running_ = true;
    while (running_) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listenSock_.fd(), &rfds);
        struct timeval tv{1, 0};
        int rc = ::select(static_cast<int>(listenSock_.fd() + 1), &rfds, nullptr, nullptr, &tv);
        if (rc < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (rc == 0) continue; // timeout

        try {
            Socket client = listenSock_.accept();
            std::lock_guard<std::mutex> lk(threadMu_);
            threads_.emplace_back([this, s = std::move(client)]() mutable {
                handleClient(std::move(s));
            });
        } catch (const SocketException& e) {
            if (running_) std::cerr << "[server] accept error: " << e.what() << "\n";
        }
    }

    // Join completed threads
    std::lock_guard<std::mutex> lk(threadMu_);
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
    threads_.clear();
}

// ---------------------------------------------------------------------------
// stop
// ---------------------------------------------------------------------------
void TelnetServer::stop() {
    running_ = false;
}

// ---------------------------------------------------------------------------
// handleClient — per-client thread
// ---------------------------------------------------------------------------
void TelnetServer::handleClient(Socket sock) {
    ClientState cs(std::move(sock));

    // Configure supported options
    cs.proto.setSupportedLocalOptions({TelnetOpt::ECHO, TelnetOpt::SUPPRESS_GA});
    cs.proto.setSupportedRemoteOptions({TelnetOpt::TERMINAL_TYPE, TelnetOpt::SUPPRESS_GA});

    // Send initial negotiation
    sendNegotiation(cs);

    // Initial Login Banner
    sendResponse(cs,
        "\r\n"
        "====================================================\r\n"
        "   Telnet Application Server (Academic Demo C++17) \r\n"
        "====================================================\r\n"
        "Authorized access only.\r\n\r\n"
        "Username: "
    );

    uint8_t buf[4096];
    while (cs.alive && running_) {
        ssize_t n = cs.sock.recv(buf, sizeof(buf));
        if (n <= 0) {
            cs.alive = false;
            break;
        }

        std::vector<uint8_t> raw(buf, buf + n);
        processIncoming(cs, raw);

        if (cs.session.shouldExit()) {
            cs.alive = false;
        }
    }
}

// ---------------------------------------------------------------------------
// sendNegotiation — announce server capabilities
// ---------------------------------------------------------------------------
void TelnetServer::sendNegotiation(ClientState& cs) {
    // WILL ECHO: server will echo
    auto w1 = cs.proto.requestWill(TelnetOpt::ECHO);
    if (!w1.empty()) cs.sock.sendAll(w1);

    // WILL SUPPRESS-GA
    auto w2 = cs.proto.requestWill(TelnetOpt::SUPPRESS_GA);
    if (!w2.empty()) cs.sock.sendAll(w2);

    // DO TERMINAL-TYPE: ask client for terminal type
    auto d1 = cs.proto.requestDo(TelnetOpt::TERMINAL_TYPE);
    if (!d1.empty()) cs.sock.sendAll(d1);
}

// ---------------------------------------------------------------------------
// processIncoming — route TelnetEvents and authenticate
// ---------------------------------------------------------------------------
void TelnetServer::processIncoming(ClientState& cs, const std::vector<uint8_t>& data) {
    auto events = cs.proto.feed(data);

    for (auto& ev : events) {
        switch (ev.action) {
        case TelnetAction::DATA: {
            // Decode NVT and append to input buffer
            auto chunk = cs.nvtDec.decode(
                std::vector<uint8_t>(ev.text.begin(), ev.text.end()));
            cs.inputBuffer += chunk.text;

            // Process any complete lines
            size_t pos;
            while ((pos = cs.inputBuffer.find('\n')) != std::string::npos) {
                std::string line = cs.inputBuffer.substr(0, pos);
                cs.inputBuffer.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();

                // Authentication state machine
                if (cs.authState == AuthState::AWAITING_USERNAME) {
                    size_t first = line.find_first_not_of(" \t\r\n");
                    std::string u = (first == std::string::npos) ? "" : line.substr(first, line.find_last_not_of(" \t\r\n") - first + 1);
                    if (u.empty()) {
                        sendResponse(cs, "Username: ");
                        continue;
                    }
                    cs.attemptedUser = u;
                    cs.authState = AuthState::AWAITING_PASSWORD;
                    // IAC WONT ECHO: tell client to suppress its own echo (password masking)
                    static const std::vector<uint8_t> wontEcho = {0xFF, 0xFC, 0x01};
                    cs.sock.sendAll(wontEcho);
                    sendResponse(cs, "Password: ");
                }
                else if (cs.authState == AuthState::AWAITING_PASSWORD) {
                    std::string p = line;
                    while (!p.empty() && (p.back() == '\r' || p.back() == '\n')) {
                        p.pop_back();
                    }

                    // Credentials: 4 authorized users
                    static const std::vector<std::pair<std::string, std::string>> credentials = {
                        {"Prosenjit", "Prosenjit123"},
                        {"Sayak",     "Sayak123"},
                        {"Sahid",     "Sahid123"},
                        {"Snehasis",  "Snehasis123"}
                    };

                    bool ok = false;
                    std::string canonicalUser;

                    for (const auto& cred : credentials) {
                        std::string u1 = cred.first;
                        std::string u2 = cs.attemptedUser;
                        for (char& c : u1) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        for (char& c : u2) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                        if (u1 == u2 && cred.second == p) {
                            ok = true;
                            canonicalUser = cred.first;
                            break;
                        }
                    }

                    if (ok) {
                        cs.authState = AuthState::AUTHENTICATED;
                        cs.authenticatedUser = canonicalUser;
                        cs.failedAttempts = 0;
                        // IAC WILL ECHO: server resumes echoing
                        static const std::vector<uint8_t> willEcho = {0xFF, 0xFB, 0x01};
                        cs.sock.sendAll(willEcho);
                        sendResponse(cs,
                            "\r\n[+] Authentication successful! Welcome, " + canonicalUser + ".\r\n"
                            "Type 'help' to view available commands.\r\n\r\n"
                        );
                        sendPrompt(cs);
                    } else {
                        cs.failedAttempts++;
                        // IAC WILL ECHO: restore echoing before showing username prompt
                        static const std::vector<uint8_t> willEchoFail = {0xFF, 0xFB, 0x01};
                        cs.sock.sendAll(willEchoFail);
                        if (cs.failedAttempts >= 3) {
                            sendResponse(cs, "\r\n[-] Maximum login attempts exceeded. Connection closed.\r\n");
                            cs.alive = false;
                            return;
                        } else {
                            sendResponse(cs,
                                "\r\n[-] Invalid username or password. Please try again.\r\n\r\n"
                                "Username: "
                            );
                            cs.authState = AuthState::AWAITING_USERNAME;
                            cs.attemptedUser.clear();
                        }
                    }
                }
                else {
                    // Authenticated command processing
                    std::string resp = cs.session.processCommand(line);
                    if (!resp.empty()) sendResponse(cs, resp);
                    if (cs.session.shouldExit()) {
                        cs.alive = false;
                        return;
                    }
                    sendPrompt(cs);
                }
            }
            break;
        }
        case TelnetAction::WILL: {
            auto resp = cs.proto.handleWill(ev.option);
            if (!resp.empty()) cs.sock.sendAll(resp);
            // If client confirmed TERMINAL_TYPE, request the value
            if (ev.option == TelnetOpt::TERMINAL_TYPE && cs.proto.isRemoteEnabled(TelnetOpt::TERMINAL_TYPE)) {
                std::vector<uint8_t> sb_send = {1}; // SEND
                auto req = cs.proto.buildSB(TelnetOpt::TERMINAL_TYPE, sb_send);
                cs.sock.sendAll(req);
            }
            break;
        }
        case TelnetAction::WONT: {
            auto resp = cs.proto.handleWont(ev.option);
            if (!resp.empty()) cs.sock.sendAll(resp);
            break;
        }
        case TelnetAction::DO: {
            auto resp = cs.proto.handleDo(ev.option);
            if (!resp.empty()) cs.sock.sendAll(resp);
            break;
        }
        case TelnetAction::DONT: {
            auto resp = cs.proto.handleDont(ev.option);
            if (!resp.empty()) cs.sock.sendAll(resp);
            break;
        }
        case TelnetAction::SUBNEG: {
            // Handle TERMINAL_TYPE IS response
            if (ev.option == TelnetOpt::TERMINAL_TYPE && !ev.subdata.empty()) {
                if (ev.subdata[0] == 0) { // IS
                    std::string termType(ev.subdata.begin() + 1, ev.subdata.end());
                    (void)termType;
                }
            }
            break;
        }
        case TelnetAction::COMMAND:
        case TelnetAction::NONE:
        default:
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// sendPrompt
// ---------------------------------------------------------------------------
void TelnetServer::sendPrompt(ClientState& cs) {
    std::string prompt = cs.authenticatedUser.empty() ? "telnet> " : (cs.authenticatedUser + "@telnet> ");
    auto encoded = cs.nvtEnc.encode(prompt);
    cs.sock.sendAll(encoded);
}

// ---------------------------------------------------------------------------
// sendResponse
// ---------------------------------------------------------------------------
void TelnetServer::sendResponse(ClientState& cs, const std::string& text) {
    auto encoded = cs.nvtEnc.encode(text);
    cs.sock.sendAll(encoded);
}
