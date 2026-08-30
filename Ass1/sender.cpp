#include <iostream>
#include <fstream>
#include <string>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "checkSum.h"
#include "crc.h"
#include "errorInjection.h"
#include "socketCommon.h"
#include "frame.h"

using namespace std;
using namespace chrono;
// sender
string charToBinary(char c)
{
    string result = "";

    for (int i = 7; i >= 0; i--)
        result += ((c >> i) & 1) ? '1' : '0';

    return result;
}

string fileToBinary(string filename)
{
    ifstream file(filename, ios::binary);

    if (!file)
    {
        cout << "Cannot open input file.\n";
        return "";
    }

    string binary;
    char c;

    while (file.get(c))
        binary += charToBinary(c);

    return binary;
}

int main()
{
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    string filename;
    int method;
    int crcChoice = 0;
    int errorMode;

    cout << "====================================\n";
    cout << "              SENDER\n";
    cout << "====================================\n";

    cout << "Enter input file: ";
    cin >> filename;

    string data = fileToBinary(filename);

    if (data.empty())
        return 1;

    cout << "\n========== FCS METHOD ==========\n";
    cout << "1. Checksum\n";
    cout << "2. CRC\n";
    cout << "Choose method: ";
    cin >> method;

    string generator = "";
    string crcName = "";

    if (method == 2)
    {
        cout << "\n========== CRC TYPES ==========\n";
        cout << "1. CRC-8\n";
        cout << "2. CRC-10\n";
        cout << "3. CRC-16\n";
        cout << "4. CRC-32\n";
        cout << "Choose CRC type: ";
        cin >> crcChoice;

        generator = getCRCGenerator(crcChoice);
        crcName = getCRCName(crcChoice);

        if (generator == "")
        {
            cout << "Invalid CRC type.\n";
            return 1;
        }

        cout << "Selected: " << crcName << endl;
        cout << "Generator: " << generator << endl;
    }

    cout << "\n========== ERROR INJECTOR ==========\n";
    cout << "1. Manual Error Injection\n";
    cout << "2. Random Error Injection\n";
    cout << "Choose mode: ";
    cin >> errorMode;

    int randomType = 0;

    if (errorMode == 2)
    {
        cout << "\n========== RANDOM ERROR TYPES ==========\n";
        cout << "1. Single Bit Error\n";
        cout << "2. Two Bit Error\n";
        cout << "3. Odd Bit Error\n";
        cout << "4. Burst Error\n";
        cout << "Choose type: ";
        cin >> randomType;
    }

    int fcsSizeInBits = 0;
    if (method == 1) fcsSizeInBits = FrameConstants::CHECKSUM16_SIZE * 8; 
    else if (crcChoice == 1) fcsSizeInBits = FrameConstants::CRC8_SIZE * 8; 
    else if (crcChoice == 2) fcsSizeInBits = FrameConstants::CRC10_SIZE * 8; 
    else if (crcChoice == 3) fcsSizeInBits = FrameConstants::CRC16_SIZE * 8; 
    else if (crcChoice == 4) fcsSizeInBits = FrameConstants::CRC32_SIZE * 8; 

    // Padding payload to ensure frame >= 64 bytes
    int minPayloadBits = (FrameConstants::MIN_FRAME_SIZE - FrameConstants::FRAME_HEADER_SIZE) * 8 - fcsSizeInBits;
    
    // We start with at least 45 bytes (360 bits) of payload
    string payload = data;
    if (payload.length() > (FrameConstants::MAX_PAYLOAD_SIZE * 8)) {
        payload = payload.substr(0, FrameConstants::MAX_PAYLOAD_SIZE * 8); // Trim to max
    }
    
    while (payload.length() < minPayloadBits)
        payload += '0'; // Pad to minimum

    string fcs;
    string serializedFrame;

    auto start = high_resolution_clock::now();

    string srcMAC = "101010101010101010101010101010101010101010101010"; // 48 bits
    string destMAC = "110011001100110011001100110011001100110011001100"; // 48 bits
    string lengthField = "0000000000101110"; // 16 bits
    string seqField = "00000001"; // 8 bits

    Header header(srcMAC, destMAC, lengthField, seqField);
    Payload framePayload(payload);
    
    string headerAndPayload = header.srcMAC + header.destMAC + header.lengthField + header.seqField + payload;

    if (method == 1)
    {
        fcs = generateChecksum(headerAndPayload);
    }
    else
    {
        fcs = generateCRC(headerAndPayload, generator);
    }

    Tailer tailer(fcs);
    Frame frameObj(header, framePayload, tailer);
    
    serializedFrame = frameObj.serialize();

    auto fcsEnd = high_resolution_clock::now();

    cout << "\n========== ORIGINAL FRAME ==========\n";
    cout << "Src MAC : " << header.srcMAC << endl;
    cout << "Dest MAC: " << header.destMAC << endl;
    cout << "Payload : " << payload << endl;
    cout << "FCS     : " << fcs << endl;
    cout << "Frame   : " << serializedFrame << endl;

    // Error injection happens AFTER FCS generation.
    if (errorMode == 1)
    {
        serializedFrame = manualError(serializedFrame);
    }
    else if (errorMode == 2)
    {
        if (randomType == 1)
            serializedFrame = randomSingleBitError(serializedFrame);
        else if (randomType == 2)
            serializedFrame = randomTwoBitError(serializedFrame);
        else if (randomType == 3)
            serializedFrame = randomOddBitError(serializedFrame);
        else if (randomType == 4)
            serializedFrame = randomBurstError(serializedFrame);
        else
        {
            cout << "Invalid random error type.\n";
            return 1;
        }
    }

    cout << "\n========== FRAME TO SEND ==========\n";
    cout << serializedFrame << endl;

    SocketType sock = socket(AF_INET, SOCK_STREAM, 0);

#ifdef _WIN32
    if (sock == INVALID_SOCKET)
#else
    if (sock < 0)
#endif
    {
        cout << "Socket creation failed.\n";
        return 1;
    }

    sockaddr_in receiver{};
    receiver.sin_family = AF_INET;
    receiver.sin_port = htons(5050);
    receiver.sin_addr.s_addr = inet_addr("127.0.0.1");

    cout << "\nConnecting to receiver...\n";

    if (connect(sock, (sockaddr*)&receiver, sizeof(receiver)) < 0)
    {
        cout << "Connection failed. Start receiver first.\n";
        closeSocket(sock);
        return 1;
    }

    cout << "Connected.\n";
    cout << "Sending frame...\n";

    // method | crcChoice | frame
    string message =
        to_string(method) + "|" +
        to_string(crcChoice) + "|" +
        serializedFrame;

    auto sendStart = high_resolution_clock::now();

    sendMessage(sock, message);

    string reply;

    if (!receiveMessage(sock, reply))
    {
        cout << "No response from receiver.\n";
        closeSocket(sock);
        return 1;
    }

    auto sendEnd = high_resolution_clock::now();

    cout << "\n========== RECEIVER REPLY ==========\n";
    cout << reply << endl;

    double fcsTime =
        duration<double, milli>(fcsEnd - start).count();

    double socketTime =
        duration<double, milli>(sendEnd - sendStart).count();

    cout << "\nFCS generation time : "
         << fcsTime << " ms\n";

    cout << "Socket round-trip   : "
         << socketTime << " ms\n";

    closeSocket(sock);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
