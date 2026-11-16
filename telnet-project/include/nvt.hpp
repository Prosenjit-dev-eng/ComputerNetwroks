#pragma once
#include <string>
#include <vector>
#include <cstdint>

enum class NvtLineBreak { CRLF, CRNUL };

// Encodes local text to NVT wire format.
// CR  -> CR LF
// IAC (0xFF) -> IAC IAC (escaped)
class NvtEncoder {
public:
    std::vector<uint8_t> encode(const std::string& text) {
        std::vector<uint8_t> out;
        out.reserve(text.size() + 8);
        for (unsigned char c : text) {
            if (c == '\r') {
                out.push_back(0x0D); // CR
                out.push_back(0x0A); // LF
            } else if (c == static_cast<unsigned char>(0xFF)) {
                out.push_back(0xFF);
                out.push_back(0xFF);
            } else {
                out.push_back(c);
            }
        }
        return out;
    }

    std::vector<uint8_t> encodeLine(const std::string& line) {
        std::vector<uint8_t> out = encode(line);
        out.push_back(0x0D); // CR
        out.push_back(0x0A); // LF
        return out;
    }
};

// Stateful incremental NVT decoder. Survives TCP fragmentation.
// IAC bytes are NOT decoded here — they are stripped by TelnetProtocol first.
class NvtDecoder {
public:
    struct DecodedChunk {
        std::string text;
        bool hadCR = false;
    };
    // We dont need here text to hex conversion bcz of uint8_t
    
    DecodedChunk decode(const std::vector<uint8_t>& raw) {
        DecodedChunk chunk;
        for (uint8_t b : raw) {
            if (pendingCR_) {
                pendingCR_ = false;
                if (b == 0x00) {
                    // CR NUL -> emit CR
                    chunk.text += '\r';
                    chunk.hadCR = true;
                } else if (b == 0x0A) {
                    // CR LF -> emit newline
                    chunk.text += '\n';
                    chunk.hadCR = true;
                } else {
                    // Lone CR then other char: emit CR then process char normally
                    chunk.text += '\r';
                    chunk.hadCR = true;
                    // fall through to process b
                    if (b == 0x0D) {
                        pendingCR_ = true;
                    } else {
                        chunk.text += static_cast<char>(b);
                    }
                }
            } else {
                if (b == 0x0D) {
                    pendingCR_ = true;
                } else {
                    chunk.text += static_cast<char>(b);
                }
            }
        }
        return chunk;
    }

    void reset() { pendingCR_ = false; }

private:
    bool pendingCR_ = false;
};
