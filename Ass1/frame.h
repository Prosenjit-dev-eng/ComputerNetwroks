#ifndef FRAME_H
#define FRAME_H

#include <string>

using namespace std;

class FrameConstants {
public:
    static const unsigned int MIN_FRAME_SIZE = 64;
    static const unsigned int MAX_FRAME_SIZE = 1518;
    
    static const unsigned int MAC_ADDRESS_SIZE = 6;
    static const unsigned int LENGTH_FIELD_SIZE = 2;
    static const unsigned int SEQUENCE_FIELD_SIZE = 1;
    
    static const unsigned int FRAME_HEADER_SIZE = (2 * MAC_ADDRESS_SIZE) + LENGTH_FIELD_SIZE + SEQUENCE_FIELD_SIZE; // 15 bytes
    
    static const unsigned int CHECKSUM16_SIZE = 2;
    static const unsigned int CRC8_SIZE = 1;
    static const unsigned int CRC10_SIZE = 2; 
    static const unsigned int CRC16_SIZE = 2;
    static const unsigned int CRC32_SIZE = 4;
    
    static const unsigned int MIN_FCS_SIZE = CRC8_SIZE;
    static const unsigned int MAX_FCS_SIZE = CRC32_SIZE;
    
    // Limits (in bytes)
    static const unsigned int MIN_PAYLOAD_SIZE = MIN_FRAME_SIZE - FRAME_HEADER_SIZE - MAX_FCS_SIZE; // 45 bytes
    static const unsigned int MAX_PAYLOAD_SIZE = MAX_FRAME_SIZE - FRAME_HEADER_SIZE - MIN_FCS_SIZE; // 1502 bytes
};

class Header {
public:
    string srcMAC;      // 48 bits
    string destMAC;     // 48 bits
    string lengthField; // 16 bits
    string seqField;    // 8 bits
    
    Header(string src = "", string dest = "", string length = "", string seq = "") 
        : srcMAC(src), destMAC(dest), lengthField(length), seqField(seq) {}
};

class Payload {
public:
    string data;
    
    Payload(string d = "") : data(d) {}
};

class Tailer {
public:
    string fcs;
    
    Tailer(string f = "") : fcs(f) {}
};

class Frame {
public:
    Header header;
    Payload payload;
    Tailer tailer;
    
    Frame(Header h, Payload p, Tailer t) : header(h), payload(p), tailer(t) {}
    
    string serialize() {
        return header.srcMAC + header.destMAC + header.lengthField + header.seqField + payload.data + tailer.fcs;
    }
};

#endif
