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

    static const unsigned int CHECKSUM16_BITS = 16;
    static const unsigned int CRC8_BITS = 8;
    static const unsigned int CRC10_BITS = 10;
    static const unsigned int CRC16_BITS = 16;
    static const unsigned int CRC32_BITS = 32;
};

// Conversions
string byteToBinary(unsigned char b);
unsigned char binaryToByte(const string &bin);
string uint16ToBinary(unsigned short val);
unsigned short binaryToUint16(const string &bin);
string textToBinary(const string &text);
string binaryToText(const string &bin);
string macToBinary(const string &macHex); // Format: "AA:BB:CC:DD:EE:FF" or binary directly

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
    string fcs; // binary string of FCS
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

// ACK Frame structure
class AckFrame {
public:
    int ackSeq;        // Sequence number being acknowledged
    bool isNack;       // true if NACK (if used), false for ACK
    string fcs;        // CRC/Checksum over ACK content

    AckFrame(int seq = 0, bool nack = false) : ackSeq(seq), isNack(nack) {}

    string serialize(int fcsMethod = 2, int crcChoice = 4) const;
    static bool deserialize(const string &msg, AckFrame &outAck, int fcsMethod = 2, int crcChoice = 4);
};

#endif
