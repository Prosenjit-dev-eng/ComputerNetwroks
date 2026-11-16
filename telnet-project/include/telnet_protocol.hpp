#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <initializer_list>

// Telnet command bytes (RFC 854)
namespace TelnetCmd {
    constexpr uint8_t SE   = 0xF0; // End of subnegotiation
    constexpr uint8_t NOP  = 0xF1; // No operation
    constexpr uint8_t DM   = 0xF2; // Data mark
    constexpr uint8_t BRK  = 0xF3; // Break
    constexpr uint8_t IP   = 0xF4; // Interrupt process
    constexpr uint8_t AO   = 0xF5; // Abort output
    constexpr uint8_t AYT  = 0xF6; // Are you there
    constexpr uint8_t EC   = 0xF7; // Erase character
    constexpr uint8_t EL   = 0xF8; // Erase line
    constexpr uint8_t GA   = 0xF9; // Go ahead
    constexpr uint8_t SB   = 0xFA; // Subnegotiation begin
    constexpr uint8_t WILL = 0xFB;
    constexpr uint8_t WONT = 0xFC;
    constexpr uint8_t DO   = 0xFD;
    constexpr uint8_t DONT = 0xFE;
    constexpr uint8_t IAC  = 0xFF;
}

// Telnet option codes (RFC 857, 858, 1091, etc.)
namespace TelnetOpt {
    constexpr uint8_t ECHO          = 1;
    constexpr uint8_t SUPPRESS_GA   = 3;
    constexpr uint8_t TERMINAL_TYPE = 24;
}

enum class TelnetAction { DATA, WILL, WONT, DO, DONT, COMMAND, SUBNEG, NONE };

struct TelnetEvent {
    TelnetAction         action  = TelnetAction::NONE;
    uint8_t              option  = 0;
    uint8_t              command = 0;
    std::vector<uint8_t> subdata;
    std::string          text;
};

enum class OptionState { NO, YES, WANTNO, WANTYES };

struct OptionStatus {
    OptionState local  = OptionState::NO;
    OptionState remote = OptionState::NO;
};

class TelnetProtocol {
public:
    TelnetProtocol() = default;

    // --- Parser ---
    std::vector<TelnetEvent> feed(const std::vector<uint8_t>& data);

    // --- Raw command builders ---
    std::vector<uint8_t> buildWill(uint8_t opt) { return {TelnetCmd::IAC, TelnetCmd::WILL, opt}; }
    std::vector<uint8_t> buildWont(uint8_t opt) { return {TelnetCmd::IAC, TelnetCmd::WONT, opt}; }
    std::vector<uint8_t> buildDo  (uint8_t opt) { return {TelnetCmd::IAC, TelnetCmd::DO,   opt}; }
    std::vector<uint8_t> buildDont(uint8_t opt) { return {TelnetCmd::IAC, TelnetCmd::DONT, opt}; }
    std::vector<uint8_t> buildSB  (uint8_t opt, const std::vector<uint8_t>& payload);
    std::vector<uint8_t> buildNop() { return {TelnetCmd::IAC, TelnetCmd::NOP}; }
    std::vector<uint8_t> buildAyt() { return {TelnetCmd::IAC, TelnetCmd::AYT}; }
    std::vector<uint8_t> buildGA () { return {TelnetCmd::IAC, TelnetCmd::GA}; }

    // --- Negotiation with loop-prevention ---
    // Local side wants to enable an option (send WILL if not already)
    std::vector<uint8_t> requestWill(uint8_t opt);
    // Remote side wants to enable an option (send DO if not already)
    std::vector<uint8_t> requestDo  (uint8_t opt);

    // Handle incoming negotiation commands
    std::vector<uint8_t> handleWill(uint8_t opt);
    std::vector<uint8_t> handleWont(uint8_t opt);
    std::vector<uint8_t> handleDo  (uint8_t opt);
    std::vector<uint8_t> handleDont(uint8_t opt);

    bool isLocalEnabled (uint8_t opt) const;
    bool isRemoteEnabled(uint8_t opt) const;

    void setSupportedLocalOptions (std::initializer_list<uint8_t> opts) {
        for (auto o : opts) supportedLocal_.insert(o);
    }
    void setSupportedRemoteOptions(std::initializer_list<uint8_t> opts) {
        for (auto o : opts) supportedRemote_.insert(o);
    }

private:
    enum class ParseState { DATA, IAC_RECV, CMD_RECV, SB_RECV, SB_IAC };

    ParseState           parseState_ = ParseState::DATA;
    uint8_t              currentCmd_ = 0;
    std::vector<uint8_t> sbBuffer_;
    uint8_t              sbOption_   = 0;
    std::string          dataAccum_;

    std::map<uint8_t, OptionStatus> options_;
    std::set<uint8_t> supportedLocal_;
    std::set<uint8_t> supportedRemote_;

    OptionStatus& getOpt(uint8_t opt) { return options_[opt]; }
    const OptionStatus* peekOpt(uint8_t opt) const {
        auto it = options_.find(opt);
        return it != options_.end() ? &it->second : nullptr;
    }
};
