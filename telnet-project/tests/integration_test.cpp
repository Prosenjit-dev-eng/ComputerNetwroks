// tests/integration_test.cpp
#include "telnet_server.hpp"
#include "telnet_protocol.hpp"
#include "nvt.hpp"
#include "socket.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <cassert>
#include <string>
#include <vector>
#include <atomic>

static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    if (expr) { std::cout << "PASS: " name "\n"; ++passed; } \
    else { std::cout << "FAIL: " name "\n"; ++failed; } \
} while(0)

static constexpr uint16_t TEST_PORT = 15023;

static Socket rawConnect(uint16_t port) {
    try {
        Socket s = Socket::createTCP();
        s.connect("127.0.0.1", port);
        return s;
    } catch (const std::exception& e) {
        return Socket(INVALID_SOCKET);
    }
}

static std::vector<uint8_t> timedRead(Socket& s, int timeoutSec = 3) {
    std::vector<uint8_t> buf;
    if (!s.valid()) return buf;
    uint8_t tmp[4096];
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(s.fd(), &rfds);
    struct timeval tv{timeoutSec, 0};
    int rc = ::select(static_cast<int>(s.fd() + 1), &rfds, nullptr, nullptr, &tv);
    if (rc <= 0) return buf;
    ssize_t n = s.recv(tmp, sizeof(tmp));
    if (n > 0) buf.assign(tmp, tmp + n);
    return buf;
}

static std::string drainAndReadData(Socket& s, int rounds = 3) {
    TelnetProtocol proto;
    proto.setSupportedLocalOptions({TelnetOpt::SUPPRESS_GA, TelnetOpt::TERMINAL_TYPE});
    proto.setSupportedRemoteOptions({TelnetOpt::ECHO, TelnetOpt::SUPPRESS_GA});

    std::string text;
    for (int i = 0; i < rounds; ++i) {
        auto raw = timedRead(s, 2);
        if (raw.empty()) break;
        auto events = proto.feed(raw);
        for (auto& ev : events) {
            if (ev.action == TelnetAction::WILL || ev.action == TelnetAction::DO) {
                std::vector<uint8_t> resp;
                if (ev.action == TelnetAction::WILL) resp = proto.handleWill(ev.option);
                else                                  resp = proto.handleDo(ev.option);
                if (!resp.empty()) s.sendAll(resp);
            } else if (ev.action == TelnetAction::DONT || ev.action == TelnetAction::WONT) {
                std::vector<uint8_t> resp;
                if (ev.action == TelnetAction::DONT) resp = proto.handleDont(ev.option);
                else                                  resp = proto.handleWont(ev.option);
                if (!resp.empty()) s.sendAll(resp);
            } else if (ev.action == TelnetAction::DATA) {
                text += ev.text;
            }
        }
    }
    return text;
}

static bool sendLine(Socket& s, const std::string& line) {
    NvtEncoder enc;
    auto data = enc.encodeLine(line);
    return s.sendAll(data);
}

int main() {
    TelnetServer server(TEST_PORT);
    std::thread serverThread([&server] { server.run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // --- Test 1: Basic connection and login prompt ---
    {
        Socket s = rawConnect(TEST_PORT);
        TEST("Can connect to server", s.valid());
        if (s.valid()) {
            std::string greeting = drainAndReadData(s, 3);
            TEST("Server prompts for username", greeting.find("Username") != std::string::npos);
            s.close();
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // --- Test 2: Successful login with credentials ---
    {
        Socket s = rawConnect(TEST_PORT);
        TEST("Test2: Can connect", s.valid());
        if (s.valid()) {
            drainAndReadData(s, 2);
            sendLine(s, "Prosenjit");
            drainAndReadData(s, 2);
            sendLine(s, "Prosenjit123");
            std::string authResp = drainAndReadData(s, 3);
            TEST("Test2: Authentication successful for Prosenjit",
                 authResp.find("Authentication successful") != std::string::npos);

            // Send help command after authentication
            sendLine(s, "help");
            std::string resp = drainAndReadData(s, 3);
            TEST("Test2: help response non-empty after login", !resp.empty());
            TEST("Test2: help response contains 'help'",
                 resp.find("help") != std::string::npos || resp.find("Help") != std::string::npos);
            s.close();
        }
    }

    server.stop();
    if (serverThread.joinable()) serverThread.join();

    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return failed == 0 ? 0 : 1;
}
