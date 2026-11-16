// tests/test_protocol.cpp — Unit tests for TelnetProtocol
#include "telnet_protocol.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>

static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    if (expr) { std::cout << "PASS: " name "\n"; ++passed; } \
    else { std::cout << "FAIL: " name "\n"; ++failed; } \
} while(0)

int main() {
    // --- IAC NOP parsing ---
    {
        TelnetProtocol p;
        std::vector<uint8_t> data{0xFF, 0xF1}; // IAC NOP
        auto events = p.feed(data);
        bool found = false;
        for (auto& ev : events)
            if (ev.action == TelnetAction::COMMAND && ev.command == 0xF1) found = true;
        TEST("IAC NOP emits COMMAND event", found);
    }

    // --- WILL option event ---
    {
        TelnetProtocol p;
        std::vector<uint8_t> data{0xFF, 0xFB, 0x01}; // IAC WILL ECHO
        auto events = p.feed(data);
        bool found = false;
        for (auto& ev : events)
            if (ev.action == TelnetAction::WILL && ev.option == 0x01) found = true;
        TEST("IAC WILL ECHO emits WILL event", found);
    }

    // --- WONT option event ---
    {
        TelnetProtocol p;
        std::vector<uint8_t> data{0xFF, 0xFC, 0x03}; // IAC WONT SGA
        auto events = p.feed(data);
        bool found = false;
        for (auto& ev : events)
            if (ev.action == TelnetAction::WONT && ev.option == 0x03) found = true;
        TEST("IAC WONT SGA emits WONT event", found);
    }

    // --- DO option event ---
    {
        TelnetProtocol p;
        std::vector<uint8_t> data{0xFF, 0xFD, 0x18}; // IAC DO TERMINAL_TYPE
        auto events = p.feed(data);
        bool found = false;
        for (auto& ev : events)
            if (ev.action == TelnetAction::DO && ev.option == 0x18) found = true;
        TEST("IAC DO TERMINAL_TYPE emits DO event", found);
    }

    // --- DONT option event ---
    {
        TelnetProtocol p;
        std::vector<uint8_t> data{0xFF, 0xFE, 0x01}; // IAC DONT ECHO
        auto events = p.feed(data);
        bool found = false;
        for (auto& ev : events)
            if (ev.action == TelnetAction::DONT && ev.option == 0x01) found = true;
        TEST("IAC DONT ECHO emits DONT event", found);
    }

    // --- Negotiation response: handleDo when supported ---
    {
        TelnetProtocol p;
        p.setSupportedLocalOptions({TelnetOpt::ECHO});
        auto resp = p.handleDo(TelnetOpt::ECHO);
        // Should respond WILL ECHO
        bool ok = resp.size() == 3 && resp[0] == 0xFF && resp[1] == 0xFB && resp[2] == 0x01;
        TEST("handleDo supported option -> WILL response", ok);
        TEST("handleDo supported option -> local enabled", p.isLocalEnabled(TelnetOpt::ECHO));
    }

    // --- Negotiation response: handleDo when unsupported ---
    {
        TelnetProtocol p;
        auto resp = p.handleDo(TelnetOpt::ECHO);
        // Should respond WONT ECHO
        bool ok = resp.size() == 3 && resp[0] == 0xFF && resp[1] == 0xFC && resp[2] == 0x01;
        TEST("handleDo unsupported option -> WONT response", ok);
        TEST("handleDo unsupported -> local NOT enabled", !p.isLocalEnabled(TelnetOpt::ECHO));
    }

    // --- Negotiation: handleWill when supported ---
    {
        TelnetProtocol p;
        p.setSupportedRemoteOptions({TelnetOpt::TERMINAL_TYPE});
        auto resp = p.handleWill(TelnetOpt::TERMINAL_TYPE);
        bool ok = resp.size() == 3 && resp[0] == 0xFF && resp[1] == 0xFD && resp[2] == 0x18;
        TEST("handleWill supported remote -> DO response", ok);
        TEST("handleWill supported remote -> remote enabled", p.isRemoteEnabled(TelnetOpt::TERMINAL_TYPE));
    }

    // --- Loop prevention: requestWill twice ---
    {
        TelnetProtocol p;
        auto r1 = p.requestWill(TelnetOpt::ECHO);
        auto r2 = p.requestWill(TelnetOpt::ECHO);
        TEST("requestWill twice: first has bytes", !r1.empty());
        TEST("requestWill twice: second is empty (loop prevention)", r2.empty());
    }

    // --- Loop prevention: requestDo twice ---
    {
        TelnetProtocol p;
        auto r1 = p.requestDo(TelnetOpt::TERMINAL_TYPE);
        auto r2 = p.requestDo(TelnetOpt::TERMINAL_TYPE);
        TEST("requestDo twice: first has bytes", !r1.empty());
        TEST("requestDo twice: second is empty (loop prevention)", r2.empty());
    }

    // --- SB/SE subneg parsing ---
    {
        TelnetProtocol p;
        // IAC SB TERMINAL_TYPE 0 'V' 'T' '1' '0' '0' IAC SE
        std::vector<uint8_t> data{
            0xFF, 0xFA, 0x18, 0x00, 'V','T','1','0','0', 0xFF, 0xF0
        };
        auto events = p.feed(data);
        bool found = false;
        for (auto& ev : events) {
            if (ev.action == TelnetAction::SUBNEG && ev.option == 0x18) {
                // subdata[0]=0 (IS), rest = "VT100"
                if (!ev.subdata.empty() && ev.subdata[0] == 0) {
                    std::string term(ev.subdata.begin() + 1, ev.subdata.end());
                    if (term == "VT100") found = true;
                }
            }
        }
        TEST("SB TERMINAL_TYPE IS VT100 SE parsed", found);
    }

    // --- IAC escape inside SB ---
    {
        TelnetProtocol p;
        // IAC SB 0x18 0xFF 0xFF 'x' IAC SE
        std::vector<uint8_t> data{
            0xFF, 0xFA, 0x18, 0xFF, 0xFF, 'x', 0xFF, 0xF0
        };
        auto events = p.feed(data);
        bool found = false;
        for (auto& ev : events) {
            if (ev.action == TelnetAction::SUBNEG && ev.option == 0x18) {
                // subdata should be [0xFF, 'x']
                if (ev.subdata.size() == 2 && ev.subdata[0] == 0xFF && ev.subdata[1] == 'x')
                    found = true;
            }
        }
        TEST("IAC IAC inside SB decoded as single 0xFF", found);
    }

    // --- Parser state survives recv() boundary ---
    {
        TelnetProtocol p;
        // Split IAC NOP across two feeds
        std::vector<uint8_t> part1{0xFF};
        std::vector<uint8_t> part2{0xF1};
        auto e1 = p.feed(part1);
        auto e2 = p.feed(part2);
        bool found = false;
        for (auto& ev : e2)
            if (ev.action == TelnetAction::COMMAND && ev.command == 0xF1) found = true;
        TEST("Parser survives IAC split across two feeds", found);
    }

    // --- Unsupported option rejection ---
    {
        TelnetProtocol p;
        // No supported options set — should return DONT for WILL
        auto resp = p.handleWill(0x05); // an obscure option
        bool ok = resp.size() == 3 && resp[1] == TelnetCmd::DONT;
        TEST("handleWill unsupported -> DONT response", ok);
    }

    // --- Data event from plain bytes ---
    {
        TelnetProtocol p;
        std::vector<uint8_t> data{'h','e','l','l','o'};
        auto events = p.feed(data);
        bool found = false;
        for (auto& ev : events)
            if (ev.action == TelnetAction::DATA && ev.text == "hello") found = true;
        TEST("Plain bytes produce DATA event", found);
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return failed == 0 ? 0 : 1;
}
