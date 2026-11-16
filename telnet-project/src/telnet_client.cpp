#include "telnet_client.hpp"
#include <iostream>
#include <cstring>
#include <cctype>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <conio.h>
#else
#include <sys/select.h>
#include <unistd.h>
#include <termios.h>
#endif

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
TelnetClient::TelnetClient(const std::string& host, uint16_t port)
    : host_(host), port_(port), sock_(Socket::createTCP())
{
}

TelnetClient::~TelnetClient() {
    stop();
    if (recvThread_.joinable()) recvThread_.join();
}

// ---------------------------------------------------------------------------
// run
// ---------------------------------------------------------------------------
void TelnetClient::run() {
    sock_.connect(host_, port_);
    running_ = true;

    // Advertise supported options before anything else
    proto_.setSupportedLocalOptions({TelnetOpt::SUPPRESS_GA, TelnetOpt::TERMINAL_TYPE});
    proto_.setSupportedRemoteOptions({TelnetOpt::ECHO, TelnetOpt::SUPPRESS_GA});

    sendNegotiation();

    // Start receive thread
    recvThread_ = std::thread([this]{ recvLoop(); });

    // Main thread handles stdin
    sendLoop();

    running_ = false;
    if (recvThread_.joinable()) recvThread_.join();
}

// ---------------------------------------------------------------------------
// stop
// ---------------------------------------------------------------------------
void TelnetClient::stop() {
    running_ = false;
}

// ---------------------------------------------------------------------------
// sendNegotiation
// ---------------------------------------------------------------------------
void TelnetClient::sendNegotiation() {
    // Request server echo our input
    auto d1 = proto_.requestDo(TelnetOpt::ECHO);
    if (!d1.empty()) sock_.sendAll(d1);

    // We suppress-GA
    auto w1 = proto_.requestWill(TelnetOpt::SUPPRESS_GA);
    if (!w1.empty()) sock_.sendAll(w1);

    // We support TERMINAL_TYPE
    auto w2 = proto_.requestWill(TelnetOpt::TERMINAL_TYPE);
    if (!w2.empty()) sock_.sendAll(w2);
}

// ---------------------------------------------------------------------------
// recvLoop — runs in its own thread, reads from server socket
// ---------------------------------------------------------------------------
void TelnetClient::recvLoop() {
    uint8_t buf[4096];
    while (running_) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock_.fd(), &rfds);
        struct timeval tv{1, 0};
        int rc = ::select(static_cast<int>(sock_.fd() + 1), &rfds, nullptr, nullptr, &tv);
        if (rc < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (rc == 0) continue;

        ssize_t n = sock_.recv(buf, sizeof(buf));
        if (n <= 0) {
            running_ = false;
            break;
        }

        std::vector<uint8_t> raw(buf, buf + n);
        auto events = proto_.feed(raw);

        for (auto& ev : events) {
            switch (ev.action) {
            case TelnetAction::DATA: {
                auto chunk = nvtDec_.decode(
                    std::vector<uint8_t>(ev.text.begin(), ev.text.end()));
                std::cout << chunk.text << std::flush;

                // Text-detection fallback: set echoOff as soon as "Password:"
                // is printed — guarantees flag is true before user can type
                std::string lower = chunk.text;
                for (char& c : lower)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (lower.find("password:") != std::string::npos) {
                    localEchoOff_.store(true);
                } else if (lower.find("username:") != std::string::npos ||
                           lower.find("login:")    != std::string::npos ||
                           lower.find("@telnet>")  != std::string::npos ||
                           lower.find("welcome,")  != std::string::npos) {
                    localEchoOff_.store(false);
                }
                break;
            }
            case TelnetAction::WILL: {
                auto resp = proto_.handleWill(ev.option);
                if (!resp.empty()) sock_.sendAll(resp);
                // Server WILL ECHO → server echoes again → turn off local masking
                if (ev.option == TelnetOpt::ECHO) {
                    localEchoOff_.store(false);
                }
                break;
            }
            case TelnetAction::WONT: {
                auto resp = proto_.handleWont(ev.option);
                if (!resp.empty()) sock_.sendAll(resp);
                // Server WONT ECHO → client must suppress its own echo (password mode)
                if (ev.option == TelnetOpt::ECHO) {
                    localEchoOff_.store(true);
                }
                break;
            }
            case TelnetAction::DO: {
                auto resp = proto_.handleDo(ev.option);
                if (!resp.empty()) sock_.sendAll(resp);
                break;
            }
            case TelnetAction::DONT: {
                auto resp = proto_.handleDont(ev.option);
                if (!resp.empty()) sock_.sendAll(resp);
                break;
            }
            case TelnetAction::SUBNEG: {
                // Server asks for terminal type
                if (ev.option == TelnetOpt::TERMINAL_TYPE && !ev.subdata.empty() && ev.subdata[0] == 1) {
                    // IS "xterm-256color"
                    std::string term = "xterm-256color";
                    std::vector<uint8_t> payload;
                    payload.push_back(0); // IS
                    for (char c : term) payload.push_back(static_cast<uint8_t>(c));
                    auto sb = proto_.buildSB(TelnetOpt::TERMINAL_TYPE, payload);
                    sock_.sendAll(sb);
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
    std::cout << "\n[Connection closed]\n" << std::flush;
}

// ---------------------------------------------------------------------------
// readLine — matches the getPassword() style exactly:
//   Enter (13) → submit,  Backspace (8) → erase,  else → record char
//   echoOff is checked LIVE on every keypress (atomic reference, no snapshot)
// ---------------------------------------------------------------------------
std::string TelnetClient::readLine(std::atomic<bool>& echoOff) {
    std::string line;
#ifdef _WIN32
    while (true) {
        int ch = _getch();

        if (ch == 13) {                   // Enter
            std::cout << "\r\n" << std::flush;
            break;
        }
        else if (ch == 8) {              // Backspace
            if (!line.empty()) {
                line.pop_back();
                std::cout << "\b \b" << std::flush;
            }
        }
        else if (ch == 0 || ch == 224) { // Extended key (arrows, F-keys)
            _getch();                     // consume second byte, ignore
        }
        else if (ch == 3) {              // Ctrl+C
            std::cout << "^C\r\n" << std::flush;
            running_ = false;
            break;
        }
        else {                           // Printable character
            line.push_back(static_cast<char>(ch));
            if (echoOff.load()) {        // check atomic LIVE — no snapshot race
                std::cout << '*' << std::flush;
            } else {
                std::cout << static_cast<char>(ch) << std::flush;
            }
        }

        if (!running_) break;
    }
#else
    // Linux/macOS: raw mode so we can intercept each character ourselves
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    while (running_) {
        char ch = 0;
        if (::read(STDIN_FILENO, &ch, 1) <= 0) break;
        if (ch == '\n' || ch == '\r') {
            std::cout << "\r\n" << std::flush;
            break;
        }
        if (ch == 127 || ch == '\b') {
            if (!line.empty()) {
                line.pop_back();
                std::cout << "\b \b" << std::flush;
            }
        } else {
            line.push_back(ch);
            if (echoOff.load()) {
                std::cout << '*' << std::flush;
            } else {
                std::cout << ch << std::flush;
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
    return line;
}

// ---------------------------------------------------------------------------
// sendLoop — main thread, reads user input and transmits to server
// ---------------------------------------------------------------------------
void TelnetClient::sendLoop() {
    while (running_) {
        // Pass the atomic ref — readLine checks it on every keypress
        std::string line = readLine(localEchoOff_);
        if (!running_) break;

        auto encoded = nvtEnc_.encodeLine(line);
        if (!sock_.sendAll(encoded)) {
            running_ = false;
            break;
        }
    }
}
