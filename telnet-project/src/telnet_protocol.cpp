#include "telnet_protocol.hpp"

// ---------------------------------------------------------------------------
// buildSB — build a subnegotiation sequence, escaping embedded IAC bytes
// ---------------------------------------------------------------------------
std::vector<uint8_t> TelnetProtocol::buildSB(uint8_t opt, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> out;
    out.reserve(payload.size() + 6);
    out.push_back(TelnetCmd::IAC);
    out.push_back(TelnetCmd::SB);
    out.push_back(opt);
    for (uint8_t b : payload) {
        out.push_back(b);
        if (b == TelnetCmd::IAC) out.push_back(TelnetCmd::IAC); // escape IAC
    }
    out.push_back(TelnetCmd::IAC);
    out.push_back(TelnetCmd::SE);
    return out;
}

// ---------------------------------------------------------------------------
// feed — incremental IAC state-machine parser (RFC 854)
// State survives across recv() calls; handles TCP fragmentation.
// ---------------------------------------------------------------------------
std::vector<TelnetEvent> TelnetProtocol::feed(const std::vector<uint8_t>& data) {
    std::vector<TelnetEvent> events;

    for (uint8_t b : data) {
        switch (parseState_) {

        // ── Normal data ──────────────────────────────────────────────────────
        case ParseState::DATA:
            if (b == TelnetCmd::IAC) {
                // Flush accumulated application data before entering command
                if (!dataAccum_.empty()) {
                    TelnetEvent ev;
                    ev.action = TelnetAction::DATA;
                    ev.text   = std::move(dataAccum_);
                    dataAccum_.clear();
                    events.push_back(std::move(ev));
                }
                parseState_ = ParseState::IAC_RECV;
            } else {
                dataAccum_ += static_cast<char>(b);
            }
            break;

        // ── Received IAC — waiting for command byte ──────────────────────────
        case ParseState::IAC_RECV:
            switch (b) {
            case TelnetCmd::WILL:
            case TelnetCmd::WONT:
            case TelnetCmd::DO:
            case TelnetCmd::DONT:
                currentCmd_ = b;
                parseState_ = ParseState::CMD_RECV;
                break;
            case TelnetCmd::SB:
                // Start of subnegotiation; sbBuffer_ accumulates option+payload
                sbBuffer_.clear();
                parseState_ = ParseState::SB_RECV;
                break;
            case TelnetCmd::IAC:
                // IAC IAC → literal data byte 0xFF
                dataAccum_ += static_cast<char>(0xFF);
                parseState_ = ParseState::DATA;
                break;
            default:
                // Single-byte command: NOP, DM, BRK, IP, AO, AYT, EC, EL, GA, SE
                {
                    TelnetEvent ev;
                    ev.action  = TelnetAction::COMMAND;
                    ev.command = b;
                    events.push_back(ev);
                    parseState_ = ParseState::DATA;
                }
                break;
            }
            break;

        // ── Received WILL/WONT/DO/DONT — waiting for option byte ─────────────
        case ParseState::CMD_RECV:
            {
                TelnetEvent ev;
                switch (currentCmd_) {
                case TelnetCmd::WILL: ev.action = TelnetAction::WILL; break;
                case TelnetCmd::WONT: ev.action = TelnetAction::WONT; break;
                case TelnetCmd::DO:   ev.action = TelnetAction::DO;   break;
                case TelnetCmd::DONT: ev.action = TelnetAction::DONT; break;
                default:              ev.action = TelnetAction::NONE; break;
                }
                ev.option = b;
                events.push_back(ev);
                parseState_ = ParseState::DATA;
            }
            break;

        // ── Inside SB, accumulating payload ──────────────────────────────────
        // sbBuffer_ stores: [option_byte, payload_byte, payload_byte, ...]
        case ParseState::SB_RECV:
            if (b == TelnetCmd::IAC) {
                parseState_ = ParseState::SB_IAC;
            } else {
                sbBuffer_.push_back(b);  // first byte will be option; rest is payload
            }
            break;

        // ── Saw IAC inside SB ─────────────────────────────────────────────────
        case ParseState::SB_IAC:
            if (b == TelnetCmd::SE) {
                // Complete subnegotiation
                TelnetEvent ev;
                ev.action = TelnetAction::SUBNEG;
                if (!sbBuffer_.empty()) {
                    ev.option  = sbBuffer_[0];
                    ev.subdata = std::vector<uint8_t>(sbBuffer_.begin() + 1, sbBuffer_.end());
                }
                events.push_back(ev);
                sbBuffer_.clear();
                parseState_ = ParseState::DATA;
            } else if (b == TelnetCmd::IAC) {
                // IAC IAC inside SB → literal 0xFF in payload
                sbBuffer_.push_back(TelnetCmd::IAC);
                parseState_ = ParseState::SB_RECV;
            } else {
                // Malformed: treat as end of SB, begin new IAC sequence
                TelnetEvent ev;
                ev.action = TelnetAction::SUBNEG;
                if (!sbBuffer_.empty()) {
                    ev.option  = sbBuffer_[0];
                    ev.subdata = std::vector<uint8_t>(sbBuffer_.begin() + 1, sbBuffer_.end());
                }
                events.push_back(ev);
                sbBuffer_.clear();
                // b is now the byte after IAC in a new command
                currentCmd_ = b;
                parseState_ = ParseState::CMD_RECV;
            }
            break;
        }
    }

    // Flush any trailing application data
    if (!dataAccum_.empty() && parseState_ == ParseState::DATA) {
        TelnetEvent ev;
        ev.action = TelnetAction::DATA;
        ev.text   = std::move(dataAccum_);
        dataAccum_.clear();
        events.push_back(std::move(ev));
    }

    return events;
}

// ---------------------------------------------------------------------------
// Negotiation helpers — RFC 1143 Q-method (loop-prevention)
// ---------------------------------------------------------------------------

// We want to send WILL <opt> (enable option on our side)
std::vector<uint8_t> TelnetProtocol::requestWill(uint8_t opt) {
    auto& s = getOpt(opt);
    if (s.local == OptionState::YES || s.local == OptionState::WANTYES)
        return {};  // already enabled or pending — don't re-send
    s.local = OptionState::WANTYES;
    return buildWill(opt);
}

// We want to send DO <opt> (request remote to enable option)
std::vector<uint8_t> TelnetProtocol::requestDo(uint8_t opt) {
    auto& s = getOpt(opt);
    if (s.remote == OptionState::YES || s.remote == OptionState::WANTYES)
        return {};  // already requested
    s.remote = OptionState::WANTYES;
    return buildDo(opt);
}

// Remote sent WILL <opt> — decide whether to accept (DO) or refuse (DONT)
std::vector<uint8_t> TelnetProtocol::handleWill(uint8_t opt) {
    auto& s = getOpt(opt);
    if (s.remote == OptionState::WANTYES) {
        // We asked for this — confirm
        s.remote = OptionState::YES;
        return {};  // no need to respond; DO was already sent
    }
    if (supportedRemote_.count(opt) && s.remote != OptionState::YES) {
        s.remote = OptionState::YES;
        return buildDo(opt);
    }
    // Unsupported or already refused
    if (s.remote != OptionState::YES) {
        s.remote = OptionState::NO;
        return buildDont(opt);
    }
    return {};
}

// Remote sent WONT <opt>
std::vector<uint8_t> TelnetProtocol::handleWont(uint8_t opt) {
    auto& s = getOpt(opt);
    std::vector<uint8_t> resp;
    if (s.remote == OptionState::YES) {
        resp = buildDont(opt);
    }
    s.remote = OptionState::NO;
    return resp;
}

// Remote sent DO <opt> — decide whether to agree (WILL) or refuse (WONT)
std::vector<uint8_t> TelnetProtocol::handleDo(uint8_t opt) {
    auto& s = getOpt(opt);
    if (s.local == OptionState::WANTYES) {
        // We already sent WILL — confirm
        s.local = OptionState::YES;
        return {};
    }
    if (supportedLocal_.count(opt) && s.local != OptionState::YES) {
        s.local = OptionState::YES;
        return buildWill(opt);
    }
    // Unsupported
    if (s.local != OptionState::YES) {
        s.local = OptionState::NO;
        return buildWont(opt);
    }
    return {};
}

// Remote sent DONT <opt>
std::vector<uint8_t> TelnetProtocol::handleDont(uint8_t opt) {
    auto& s = getOpt(opt);
    std::vector<uint8_t> resp;
    if (s.local == OptionState::YES) {
        resp = buildWont(opt);
    }
    s.local = OptionState::NO;
    return resp;
}

bool TelnetProtocol::isLocalEnabled(uint8_t opt) const {
    const auto* p = peekOpt(opt);
    return p && p->local == OptionState::YES;
}

bool TelnetProtocol::isRemoteEnabled(uint8_t opt) const {
    const auto* p = peekOpt(opt);
    return p && p->remote == OptionState::YES;
}
