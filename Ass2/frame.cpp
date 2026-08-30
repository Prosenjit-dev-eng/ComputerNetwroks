                  #include "frame.h"
#include "crc.h"
#include "checkSum.h"
#include <sstream>
#include <iomanip>
#include <iostream>

using namespace std;

string byteToBinary(unsigned char b)
{
    string res = "";
    for (int i = 7; i >= 0; i--)
        res += ((b >> i) & 1) ? '1' : '0';
    return res;
}

unsigned char binaryToByte(const string &bin)
{
    unsigned char val = 0;
    for (size_t i = 0; i < bin.length() && i < 8; i++)
    {
        val = (val << 1) | (bin[i] - '0');
    }
    return val;
}

string uint16ToBinary(unsigned short val)
{
    string res = "";
    for (int i = 15; i >= 0; i--)
        res += ((val >> i) & 1) ? '1' : '0';
    return res;
}

unsigned short binaryToUint16(const string &bin)
{
    unsigned short val = 0;
    for (size_t i = 0; i < bin.length() && i < 16; i++)
    {
        val = (val << 1) | (bin[i] - '0');
    }
    return val;
}

string textToBinary(const string &text)
{
    string binary = "";
    for (char c : text)
        binary += byteToBinary((unsigned char)c);
    return binary;
}

string binaryToText(const string &bin)
{
    string text = "";
    for (size_t i = 0; i + 8 <= bin.length(); i += 8)
    {
        text += (char)binaryToByte(bin.substr(i, 8));
    }
    return text;
}

string macToBinary(const string &macHex)
{
    // If already a 48-char binary string, return it
    if (macHex.length() == 48) {
        bool allBin = true;
        for (char c : macHex) {
            if (c != '0' && c != '1') { allBin = false; break; }
        }
        if (allBin) return macHex;
    }

    // Otherwise parse hex like "AA:BB:CC:DD:EE:FF" or "AABBCCDDEEFF"
    string cleanHex = "";
    for (char c : macHex) {
        if (isxdigit(c)) cleanHex += c;
    }
    while (cleanHex.length() < 12) cleanHex += '0';
    cleanHex = cleanHex.substr(0, 12);

    string res = "";
    for (size_t i = 0; i < 12; i += 2) {
        unsigned int byteVal;
        stringstream ss;
        ss << hex << cleanHex.substr(i, 2);
        ss >> byteVal;
        res += byteToBinary((unsigned char)byteVal);
    }
    return res;
}

int Header::getSequenceNumber() const
{
    return (int)binaryToByte(seqField);
}

int Header::getPayloadLength() const
{
    return (int)binaryToUint16(lengthField);
}

string Frame::serialize() const
{
    return header.srcMAC + header.destMAC + header.lengthField + header.seqField + payload.data + tailer.fcs;
}

string Frame::getHeaderAndPayload() const
{
    return header.srcMAC + header.destMAC + header.lengthField + header.seqField + payload.data;
}

bool Frame::deserialize(const string &bitstream, int fcsBits, Frame &outFrame)
{
    // Minimum header size: 48 + 48 + 16 + 8 = 120 bits
    const size_t headerBits = 120;
    if (bitstream.length() < headerBits + fcsBits)
        return false;

    outFrame.header.srcMAC = bitstream.substr(0, 48);
    outFrame.header.destMAC = bitstream.substr(48, 48);
    outFrame.header.lengthField = bitstream.substr(96, 16);
    outFrame.header.seqField = bitstream.substr(112, 8);

    size_t payloadBits = bitstream.length() - headerBits - fcsBits;
    outFrame.payload.data = bitstream.substr(headerBits, payloadBits);
    outFrame.tailer.fcs = bitstream.substr(bitstream.length() - fcsBits, fcsBits);

    return true;
}

string AckFrame::serialize(int fcsMethod, int crcChoice) const
{
    // Format: "ACK:" + 8-bit seq + 1-bit nack + FCS
    string seqBin = byteToBinary((unsigned char)ackSeq);
    string nackBin = isNack ? "1" : "0";
    string body = seqBin + nackBin;

    string fcsStr = "";
    if (fcsMethod == 1) {
        fcsStr = generateChecksum(body);
    } else {
        string gen = getCRCGenerator(crcChoice);
        fcsStr = generateCRC(body, gen);
    }

    return "ACK|" + to_string(ackSeq) + "|" + (isNack ? "1" : "0") + "|" + fcsStr;
}

bool AckFrame::deserialize(const string &msg, AckFrame &outAck, int fcsMethod, int crcChoice)
{
    // Expected format: "ACK|seq|isNack|fcs"
    if (msg.rfind("ACK|", 0) != 0) return false;

    size_t p1 = msg.find('|');
    size_t p2 = msg.find('|', p1 + 1);
    size_t p3 = msg.find('|', p2 + 1);

    if (p1 == string::npos || p2 == string::npos || p3 == string::npos)
        return false;

    string seqStr = msg.substr(p1 + 1, p2 - p1 - 1);
    string nackStr = msg.substr(p2 + 1, p3 - p2 - 1);
    string fcsStr = msg.substr(p3 + 1);

    outAck.ackSeq = stoi(seqStr);
    outAck.isNack = (nackStr == "1");
    outAck.fcs = fcsStr;

    // Verify FCS
    string body = byteToBinary((unsigned char)outAck.ackSeq) + (outAck.isNack ? "1" : "0");
    if (fcsMethod == 1) {
        return verifyChecksum(body, fcsStr);
    } else {
        string gen = getCRCGenerator(crcChoice);
        return verifyCRC(body + fcsStr, gen);
    }
}
