#ifndef FRAME_H
#define FRAME_H

#include <string>
#include <vector>

using namespace std;

class FrameConstants {
public:
    static const unsigned int MAC_ADDRESS_BYTES = 6;
    static const unsigned int LENGTH_FIELD_BYTES = 2;
    static const unsigned int SEQUENCE_FIELD_BYTES = 1;
    static const unsigned int FRAME_HEADER_BYTES = (2 * MAC_ADDRESS_BYTES) + LENGTH_FIELD_BYTES + SEQUENCE_FIELD_BYTES; // 15 bytes

    static const unsigned int MIN_PAYLOAD_BYTES = 46;
    static const unsigned int MAX_PAYLOAD_BYTES = 1500;

    static const unsigned int CRC32_BITS = 32;
};

// Conversions
string byteToBinary(unsigned char b);
unsigned char binaryToByte(const string &bin);
string uint16ToBinary(unsigned short val);
unsigned short binaryToUint16(const string &bin);
string textToBinary(const string &text);
string binaryToText(const string &bin);
string macToBinary(const string &macHex);

class Header {
public:
    string srcMAC;      // 48 bits
    string destMAC;     // 48 bits
    string lengthField; // 16 bits
    string seqField;    // 8 bits

    Header(string src = "", string dest = "", string length = "", string seq = "")
        : srcMAC(src), destMAC(dest), lengthField(length), seqField(seq) {}

    int getSequenceNumber() const;
    int getPayloadLength() const;
};

class Payload {
public:
    string data; // binary string of payload bits
    Payload(string d = "") : data(d) {}
};

class Tailer {
public:
    string fcs; // binary string of FCS (CRC-32)
    Tailer(string f = "") : fcs(f) {}
};

class Frame {
public:
    Header header;
    Payload payload;
    Tailer tailer;

    Frame() {}
    Frame(Header h, Payload p, Tailer t) : header(h), payload(p), tailer(t) {}

    string serialize() const;
    string getHeaderAndPayload() const;
    static bool deserialize(const string &bitstream, int fcsBits, Frame &outFrame);
};

// Built-in CRC-32 Helper Functions
string computeCRC32(const string &data);
bool verifyCRC32(const string &codeWord);

#endif
