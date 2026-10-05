#include "frame.h"
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

static string xorStrings(const string &a, const string &b) {
    string res = "";
    for (size_t i = 1; i < b.length(); i++)
        res += (a[i] == b[i]) ? '0' : '1';
    return res;
}

static string mod2Div(string dividend, const string &divisor) {
    size_t divLen = divisor.length();
    string temp = dividend.substr(0, divLen);

    while (divLen < dividend.length()) {
        if (temp[0] == '1') temp = xorStrings(divisor, temp);
        else temp = xorStrings(string(divisor.length(), '0'), temp);
        temp += dividend[divLen];
        divLen++;
    }

    if (temp[0] == '1') temp = xorStrings(divisor, temp);
    else temp = xorStrings(string(dividend.length(), '0'), temp);

    return temp;
}

string computeCRC32(const string &data) {
    const string CRC32_POLY = "100000100110000010001110110110111"; // 33-bit standard IEEE 802 CRC-32 polynomial
    string padded = data + string(32, '0');
    return mod2Div(padded, CRC32_POLY);
}

bool verifyCRC32(const string &codeWord) {
    const string CRC32_POLY = "100000100110000010001110110110111";
    string rem = mod2Div(codeWord, CRC32_POLY);
    for (char c : rem) {
        if (c == '1') return false;
    }
    return true;
}
