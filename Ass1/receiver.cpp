#include <iostream>
#include <string>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "checkSum.h"
#include "crc.h"
#include "socketCommon.h"
#include "frame.h"

using namespace std;
using namespace chrono;

int main()
{
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    cout << "====================================\n";
    cout << "             RECEIVER\n";
    cout << "====================================\n";

    SocketType serverSocket =
        socket(AF_INET, SOCK_STREAM, 0);

#ifdef _WIN32
    if (serverSocket == INVALID_SOCKET)
#else
    if (serverSocket < 0)
#endif
    {
        cout << "Socket creation failed.\n";
        return 1;
    }

    int option = 1;

    setsockopt(
        serverSocket,
        SOL_SOCKET,
        SO_REUSEADDR,
#ifdef _WIN32
        (const char*)&option,
#else
        &option,
#endif
        sizeof(option)
    );

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(5050);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket,
             (sockaddr*)&server,
             sizeof(server)) < 0)
    {
        cout << "Bind failed.\n";
        closeSocket(serverSocket);
        return 1;
    }

    listen(serverSocket, 1);

    cout << "Waiting for sender on port 5050...\n";

    SocketType clientSocket =
        accept(serverSocket, nullptr, nullptr);

#ifdef _WIN32
    if (clientSocket == INVALID_SOCKET)
#else
    if (clientSocket < 0)
#endif
    {
        cout << "Accept failed.\n";
        return 1;
    }

    cout << "Sender connected.\n";

    string message;

    if (!receiveMessage(clientSocket, message))
    {
        cout << "Frame receive failed.\n";
        closeSocket(clientSocket);
        closeSocket(serverSocket);
        return 1;
    }

    // method | crcChoice | frame
    size_t p1 = message.find('|');
    size_t p2 = message.find('|', p1 + 1);

    int method =
        stoi(message.substr(0, p1));

    int crcChoice =
        stoi(message.substr(p1 + 1, p2 - p1 - 1));

    string frame =
        message.substr(p2 + 1);

    string generator = "";

    if (method == 2)
    {
        generator = getCRCGenerator(crcChoice);

        cout << "\nCRC Type : "
             << getCRCName(crcChoice) << endl;

        cout << "Generator: "
             << generator << endl;
    }

    cout << "\n========== RECEIVED FRAME ==========\n";
    cout << frame << endl;

    int fcsLength = 0;
    if (method == 1) fcsLength = FrameConstants::CHECKSUM16_SIZE * 8; 
    else if (crcChoice == 1) fcsLength = FrameConstants::CRC8_SIZE * 8; 
    else if (crcChoice == 2) fcsLength = FrameConstants::CRC10_SIZE * 8; 
    else if (crcChoice == 3) fcsLength = FrameConstants::CRC16_SIZE * 8; 
    else if (crcChoice == 4) fcsLength = FrameConstants::CRC32_SIZE * 8; 

    int headerLength = FrameConstants::FRAME_HEADER_SIZE * 8;

    if (frame.length() < FrameConstants::MIN_FRAME_SIZE * 8) {
        cout << "Warning: Received frame is smaller than minimum frame size.\n";
    }

    string headerStr = frame.substr(0, headerLength);
    string srcMAC = headerStr.substr(0, 48);
    string destMAC = headerStr.substr(48, 48);
    string lengthField = headerStr.substr(96, 16);
    string seqField = headerStr.substr(112, 8);

    string data =
        frame.substr(headerLength, frame.length() - headerLength - fcsLength);

    string receivedFCS =
        frame.substr(frame.length() - fcsLength);

    cout << "\nSrc MAC : " << srcMAC << endl;
    cout << "Dest MAC: " << destMAC << endl;
    cout << "Payload : " << data << endl;
    cout << "FCS     : " << receivedFCS << endl;
    
    Header header(srcMAC, destMAC, lengthField, seqField);
    Payload payloadObj(data);
    Tailer tailer(receivedFCS);
    Frame frameObj(header, payloadObj, tailer);

    auto start = high_resolution_clock::now();

    bool valid;
    string headerAndPayload = headerStr + data;
    string frameToVerifyCRC = headerAndPayload + receivedFCS;

    if (method == 1)
        valid = verifyChecksum(headerAndPayload, receivedFCS);
    else
        valid = verifyCRC(frameToVerifyCRC, generator);

    auto end = high_resolution_clock::now();

    cout << "\n========== RESULT ==========\n";

    if (valid)
        cout << "ACCEPT - No error detected\n";
    else
        cout << "REJECT - Error detected\n";

    double receiverTime =
        duration<double, milli>(end - start).count();

    cout << "Receiver FCS verification time : "
         << receiverTime << " ms\n";

    sendMessage(
        clientSocket,
        valid ? "ACCEPT - No error detected"
              : "REJECT - Error detected"
    );

    closeSocket(clientSocket);
    closeSocket(serverSocket);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
