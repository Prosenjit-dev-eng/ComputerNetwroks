#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <chrono>

#include "socketCommon.h"
#include "frame.h"
#include "crc.h"
#include "checkSum.h"
#include "channel.h"

using namespace std;
using namespace chrono;

int main()
{
    initializeSocketSubsystem();

    cout << "====================================================\n";
    cout << "          DATA LINK LAYER RECEIVER (ARQ)            \n";
    cout << "====================================================\n";

    int protocol = 1;
    cout << "Select Flow Control Protocol:\n";
    cout << "  1. Stop-and-Wait ARQ\n";
    cout << "  2. Go-Back-N ARQ\n";
    cout << "  3. Selective Repeat ARQ\n";
    cout << "Enter choice (1-3): ";
    if (!(cin >> protocol)) protocol = 1;

    int windowSize = 4;
    if (protocol == 2 || protocol == 3)
    {
        cout << "Enter Receiver Window Size N (e.g., 4): ";
        cin >> windowSize;
        if (windowSize < 1) windowSize = 1;
    }
    else
    {
        windowSize = 1;
    }

    int fcsMethod = 2;
    int crcChoice = 4;
    cout << "\nSelect FCS Verification Method:\n";
    cout << "  1. Checksum (16-bit)\n";
    cout << "  2. CRC\n";
    cout << "Enter choice (1-2): ";
    cin >> fcsMethod;

    if (fcsMethod == 2)
    {
        cout << "Select CRC Type (1: CRC-8, 2: CRC-10, 3: CRC-16, 4: CRC-32): ";
        cin >> crcChoice;
        if (crcChoice < 1 || crcChoice > 4) crcChoice = 4;
    }

    int fcsBits = 32;
    if (fcsMethod == 1) fcsBits = FrameConstants::CHECKSUM16_BITS;
    else if (crcChoice == 1) fcsBits = FrameConstants::CRC8_BITS;
    else if (crcChoice == 2) fcsBits = FrameConstants::CRC10_BITS;
    else if (crcChoice == 3) fcsBits = FrameConstants::CRC16_BITS;
    else if (crcChoice == 4) fcsBits = FrameConstants::CRC32_BITS;

    double ackLossProb = 0.0;
    double ackErrProb = 0.0;
    cout << "\nSimulate ACK channel unreliability? (Enter ACK Loss prob [0.0 - 0.5]): ";
    cin >> ackLossProb;
    cout << "Enter ACK Error prob [0.0 - 0.5]: ";
    cin >> ackErrProb;

    Channel ackChannel(ChannelConfig(ackLossProb, ackErrProb, 1, 2, 10, true));

    int port = 5050;
    SocketType recvSock = createUdpSocket();
    if (IS_INVALID_SOCKET(recvSock))
    {
        cerr << "Failed to create UDP socket.\n";
        return 1;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(recvSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(recvSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bool bound = false;
    for (int p = 5050; p <= 5060; p++)
    {
        serverAddr.sin_port = htons(p);
        if (bind(recvSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == 0)
        {
            port = p;
            bound = true;
            break;
        }
    }

    if (!bound)
    {
        cerr << "Socket bind failed across ports 5050-5060. Please close running sender/receiver terminal instances.\n";
        closeSocket(recvSock);
        return 1;
    }

    cout << "\n====================================================\n";
    cout << "  [RECEIVER ACTIVE] Listening on UDP Port: " << port << "\n";
    cout << "====================================================\n";
    cout << "Protocol: " << (protocol == 1 ? "Stop-and-Wait" : (protocol == 2 ? "Go-Back-N" : "Selective Repeat"))
         << " | Window N = " << windowSize << " | FCS = " << (fcsMethod == 1 ? "Checksum" : getCRCName(crcChoice)) << "\n\n";

    int expectedSeq = 0;
    int receiverBase = 0;
    map<int, string> srBuffer; // For Selective Repeat out-of-order buffering
    string reconstructedData = "";
    sockaddr_in senderAddr{};
    bool senderKnown = false;

    long long totalFramesReceived = 0;
    long long corruptedFrames = 0;
    long long duplicateFrames = 0;
    long long validAcceptedFrames = 0;
    long long acksSent = 0;

    auto startTime = high_resolution_clock::now();
    bool transmissionComplete = false;

    while (!transmissionComplete)
    {
        string rawPacket;
        sockaddr_in from{};
        if (!recvUdp(recvSock, rawPacket, from))
        {
            continue;
        }

        if (!senderKnown)
        {
            senderAddr = from;
            senderKnown = true;
        }

        totalFramesReceived++;

        // 1. Check for EOT (End of Transmission) signal
        if (rawPacket.find("CONTROL_EOT") != string::npos)
        {
            cout << "\n[RECEIVER] Received End-of-Transmission (EOT) signal from sender.\n";
            // Send final ACK for EOT
            AckFrame eotAck(999, false);
            string ackMsg = eotAck.serialize(fcsMethod, crcChoice);
            sendUdp(recvSock, ackMsg, from);
            transmissionComplete = true;
            break;
        }

        // 2. Deserialize frame
        Frame frame;
        if (!Frame::deserialize(rawPacket, fcsBits, frame))
        {
            cout << "  [RECEIVER] Corrupted frame structure (discarding)...\n";
            corruptedFrames++;
            continue;
        }

        int seq = frame.header.getSequenceNumber();
        int payloadLen = frame.header.getPayloadLength();

        // 3. Verify FCS (CRC / Checksum)
        bool isValid = false;
        string dataToCheck = frame.getHeaderAndPayload();
        if (fcsMethod == 1)
        {
            isValid = verifyChecksum(dataToCheck, frame.tailer.fcs);
        }
        else
        {
            string gen = getCRCGenerator(crcChoice);
            isValid = verifyCRC(dataToCheck + frame.tailer.fcs, gen);
        }

        if (!isValid)
        {
            cout << "  [RECEIVER] Frame " << seq << " FCS verification FAILED (CRC/Checksum Error) -> Discarded!\n";
            corruptedFrames++;
            // In Go-Back-N or Stop-and-Wait, receiver drops corrupted frame (or may send duplicate ACK)
            if (protocol == 2 && expectedSeq > 0)
            {
                // Send cumulative ACK for last correctly received frame
                AckFrame dupAck(expectedSeq - 1, false);
                string ackStr = dupAck.serialize(fcsMethod, crcChoice);
                auto transRes = ackChannel.passThrough(ackStr, true);
                if (!transRes.isDropped)
                {
                    sendUdp(recvSock, transRes.transmittedData, from);
                    acksSent++;
                }
            }
            continue;
        }

        // Frame is mathematically valid!
        // Extract original payload characters according to lengthField
        string rawPayloadBits = frame.payload.data;
        string actualText = "";
        if ((size_t)payloadLen * 8 <= rawPayloadBits.length())
        {
            actualText = binaryToText(rawPayloadBits.substr(0, payloadLen * 8));
        }
        else
        {
            actualText = binaryToText(rawPayloadBits);
        }

        // 4. Protocol Flow Control Handling
        if (protocol == 1) // STOP-AND-WAIT
        {
            if (seq == expectedSeq)
            {
                cout << "  [RECEIVER] Accepted valid Frame " << seq << " (Payload: " << payloadLen << " bytes)\n";
                reconstructedData += actualText;
                validAcceptedFrames++;
                expectedSeq++;

                // Send ACK for this sequence number
                AckFrame ack(seq, false);
                string ackStr = ack.serialize(fcsMethod, crcChoice);
                auto transRes = ackChannel.passThrough(ackStr, true);
                if (!transRes.isDropped)
                {
                    sendUdp(recvSock, transRes.transmittedData, from);
                    acksSent++;
                    cout << "  [RECEIVER] Sent ACK " << seq << "\n";
                }
            }
            else
            {
                cout << "  [RECEIVER] Duplicate Frame " << seq << " (Expected " << expectedSeq << ") -> Re-sending ACK " << (expectedSeq - 1) << "\n";
                duplicateFrames++;
                if (expectedSeq > 0)
                {
                    AckFrame ack(expectedSeq - 1, false);
                    string ackStr = ack.serialize(fcsMethod, crcChoice);
                    auto transRes = ackChannel.passThrough(ackStr, true);
                    if (!transRes.isDropped)
                    {
                        sendUdp(recvSock, transRes.transmittedData, from);
                        acksSent++;
                    }
                }
            }
        }
        else if (protocol == 2) // GO-BACK-N
        {
            if (seq == expectedSeq)
            {
                cout << "  [RECEIVER] Accepted in-order Frame " << seq << " (Payload: " << payloadLen << " bytes)\n";
                reconstructedData += actualText;
                validAcceptedFrames++;
                expectedSeq++;

                // Send cumulative ACK for expectedSeq - 1
                AckFrame ack(seq, false);
                string ackStr = ack.serialize(fcsMethod, crcChoice);
                auto transRes = ackChannel.passThrough(ackStr, true);
                if (!transRes.isDropped)
                {
                    sendUdp(recvSock, transRes.transmittedData, from);
                    acksSent++;
                    cout << "  [RECEIVER] Sent Cumulative ACK " << seq << "\n";
                }
            }
            else
            {
                cout << "  [RECEIVER] Out-of-order Frame " << seq << " (Expected " << expectedSeq << ") -> Discarded, Re-sending ACK " << (expectedSeq - 1) << "\n";
                duplicateFrames++;
                if (expectedSeq > 0)
                {
                    AckFrame ack(expectedSeq - 1, false);
                    string ackStr = ack.serialize(fcsMethod, crcChoice);
                    auto transRes = ackChannel.passThrough(ackStr, true);
                    if (!transRes.isDropped)
                    {
                        sendUdp(recvSock, transRes.transmittedData, from);
                        acksSent++;
                    }
                }
            }
        }
        else if (protocol == 3) // SELECTIVE REPEAT
        {
            // Receiver window: [receiverBase, receiverBase + windowSize - 1]
            if (seq >= receiverBase && seq < receiverBase + windowSize)
            {
                cout << "  [RECEIVER] Received in-window Frame " << seq << "\n";
                srBuffer[seq] = actualText;
                validAcceptedFrames++;

                // Send individual ACK for this frame
                AckFrame ack(seq, false);
                string ackStr = ack.serialize(fcsMethod, crcChoice);
                auto transRes = ackChannel.passThrough(ackStr, true);
                if (!transRes.isDropped)
                {
                    sendUdp(recvSock, transRes.transmittedData, from);
                    acksSent++;
                    cout << "  [RECEIVER] Sent Individual ACK " << seq << "\n";
                }

                // Slide receiver base if consecutive frames available
                while (srBuffer.find(receiverBase) != srBuffer.end())
                {
                    cout << "  [RECEIVER] Delivering Frame " << receiverBase << " to upper layer, advancing window.\n";
                    reconstructedData += srBuffer[receiverBase];
                    srBuffer.erase(receiverBase);
                    receiverBase++;
                }
            }
            else if (seq < receiverBase)
            {
                // Previously acknowledged frame, re-send ACK so sender can advance
                cout << "  [RECEIVER] Duplicate / Old Frame " << seq << " (Base is " << receiverBase << ") -> Re-ACKing\n";
                duplicateFrames++;
                AckFrame ack(seq, false);
                string ackStr = ack.serialize(fcsMethod, crcChoice);
                auto transRes = ackChannel.passThrough(ackStr, true);
                if (!transRes.isDropped)
                {
                    sendUdp(recvSock, transRes.transmittedData, from);
                    acksSent++;
                }
            }
            else
            {
                cout << "  [RECEIVER] Frame " << seq << " outside receiver window [" << receiverBase << ", " << receiverBase + windowSize - 1 << "] -> Discarded\n";
            }
        }
    }

    auto endTime = high_resolution_clock::now();
    double totalTimeMs = duration_cast<milliseconds>(endTime - startTime).count();

    cout << "\n====================================================\n";
    cout << "             TRANSMISSION RECEPTION SUMMARY         \n";
    cout << "====================================================\n";
    cout << "Total Frames Received : " << totalFramesReceived << endl;
    cout << "Valid Accepted Frames : " << validAcceptedFrames << endl;
    cout << "Corrupted Frames      : " << corruptedFrames << endl;
    cout << "Duplicate Frames      : " << duplicateFrames << endl;
    cout << "ACKs Sent             : " << acksSent << endl;
    cout << "Total Time Elapsed    : " << totalTimeMs << " ms" << endl;
    cout << "Total Reconstructed B : " << reconstructedData.length() << " bytes\n";

    // Save to output file
    ofstream outFile("received_output.txt", ios::binary);
    if (outFile)
    {
        outFile << reconstructedData;
        cout << "\nReconstructed file saved to 'received_output.txt'.\n";
    }

    cout << "\nPreview of received data:\n-----------------------------------------\n";
    if (reconstructedData.length() > 200)
    {
        cout << reconstructedData.substr(0, 200) << "... [truncated]\n";
    }
    else
    {
        cout << reconstructedData << "\n";
    }
    cout << "-----------------------------------------\n";

    closeSocket(recvSock);
    cleanupSocketSubsystem();
    return 0;
}
