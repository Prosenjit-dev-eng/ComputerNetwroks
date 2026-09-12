#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>

#include "socketCommon.h"
#include "frame.h"
#include "crc.h"
#include "checkSum.h"
#include "errorInjection.h"
#include "channel.h"
#include "rttEstimator.h"

using namespace std;
using namespace chrono;

struct PacketInfo {
    int seqNum;
    Frame frameObj;
    string serializedData;
    time_point<high_resolution_clock> sendTime;
    int transmitCount;
    bool isAcked;
};

int main()
{
    initializeSocketSubsystem();

    cout << "====================================================\n";
    cout << "           DATA LINK LAYER SENDER (ARQ)             \n";
    cout << "====================================================\n";

    string filename;
    cout << "Enter input text file (or press enter for 'input.txt'): ";
    getline(cin, filename);
    if (filename.empty()) filename = "input.txt";

    ifstream inFile(filename, ios::binary);
    if (!inFile)
    {
        cerr << "Cannot open file: " << filename << ". Creating sample file...\n";
        ofstream sampleFile("input.txt");
        sampleFile << "Hello! This is a test message for Data Link Layer ARQ Flow Control Simulation.\n"
                   << "Testing Stop-and-Wait, Go-Back-N, and Selective Repeat protocols under random bit errors and delay.\n"
                   << "Data Link Layer frames include MAC headers, Length, Sequence numbers, Payload, and CRC/Checksum FCS.\n";
        sampleFile.close();
        filename = "input.txt";
        inFile.open(filename, ios::binary);
    }

    string fileContent((istreambuf_iterator<char>(inFile)), istreambuf_iterator<char>());
    inFile.close();

    cout << "Loaded file '" << filename << "' (" << fileContent.length() << " bytes).\n";

    int protocol = 1;
    cout << "\nSelect Flow Control Protocol:\n";
    cout << "  1. Stop-and-Wait ARQ\n";
    cout << "  2. Go-Back-N ARQ\n";
    cout << "  3. Selective Repeat ARQ\n";
    cout << "Enter choice (1-3): ";
    cin >> protocol;

    int windowSize = 1;
    if (protocol == 2 || protocol == 3)
    {
        cout << "Enter Sender Window Size N (e.g., 4): ";
        cin >> windowSize;
        if (windowSize < 1) windowSize = 1;
    }

    int payloadChunkSize = 46;
    cout << "\nEnter Payload size per frame (46 - 1500 bytes, default 46): ";
    cin >> payloadChunkSize;
    if (payloadChunkSize < (int)FrameConstants::MIN_PAYLOAD_BYTES)
        payloadChunkSize = FrameConstants::MIN_PAYLOAD_BYTES;
    if (payloadChunkSize > (int)FrameConstants::MAX_PAYLOAD_BYTES)
        payloadChunkSize = FrameConstants::MAX_PAYLOAD_BYTES;

    int fcsMethod = 2;
    int crcChoice = 4;
    cout << "\nSelect FCS Method:\n";
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

    cout << "\n========== CHANNEL UNRELIABILITY SETUP ==========\n";
    double lossProb = 0.1;
    double errProb = 0.1;
    cout << "Enter Frame Loss Probability [0.0 - 0.5]: ";
    cin >> lossProb;
    cout << "Enter Frame Error/Corruption Probability [0.0 - 0.5]: ";
    cin >> errProb;

    int errorType = 1;
    if (errProb > 0.0)
    {
        cout << "Select Error Type (0: Probabilistic, 1: Single bit, 2: Two bit, 3: Odd bit, 4: Burst): ";
        cin >> errorType;
    }

    int minDelay = 10, maxDelay = 30;
    cout << "Enter Min Propagation Delay (ms): ";
    cin >> minDelay;
    cout << "Enter Max Propagation Delay (ms): ";
    cin >> maxDelay;

    Channel channel(ChannelConfig(lossProb, errProb, errorType, minDelay, maxDelay, true));
    RttEstimator rttEstimator(200.0, 30.0, 3000.0);

    // Setup Socket
    SocketType sendSock = createUdpSocket();
    if (IS_INVALID_SOCKET(sendSock))
    {
        cerr << "Failed to create UDP socket.\n";
        return 1;
    }
    setSocketNonBlocking(sendSock, true);

    int receiverPort = 5050;
    cout << "Enter Receiver UDP Port (default 5050): ";
    if (!(cin >> receiverPort)) receiverPort = 5050;

    sockaddr_in destAddr{};
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(receiverPort);
#ifdef _WIN32
    destAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
#else
    inet_pton(AF_INET, "127.0.0.1", &destAddr.sin_addr);
#endif

    // MAC Addresses
    string srcMAC = macToBinary("00:1A:2B:3C:4D:5E");
    string destMAC = macToBinary("00:5E:4D:3C:2B:1A");

    // 1. Prepare all Frames
    vector<PacketInfo> packets;
    int seqNum = 0;
    for (size_t offset = 0; offset < fileContent.length() || (offset == 0 && fileContent.empty()); offset += payloadChunkSize)
    {
        string chunk = fileContent.substr(offset, payloadChunkSize);
        int chunkLen = (int)chunk.length();

        // Convert chunk to binary string
        string chunkBin = textToBinary(chunk);
        // Pad to min payload size if necessary
        while (chunkBin.length() < FrameConstants::MIN_PAYLOAD_BYTES * 8)
        {
            chunkBin += "00000000"; // pad zero byte
        }

        string lenBin = uint16ToBinary((unsigned short)chunkLen);
        string seqBin = byteToBinary((unsigned char)(seqNum % 256));

        Header header(srcMAC, destMAC, lenBin, seqBin);
        Payload payload(chunkBin);

        string headerAndPayload = header.srcMAC + header.destMAC + header.lengthField + header.seqField + payload.data;

        string fcs = "";
        if (fcsMethod == 1)
        {
            fcs = generateChecksum(headerAndPayload);
        }
        else
        {
            string gen = getCRCGenerator(crcChoice);
            fcs = generateCRC(headerAndPayload, gen);
        }

        Tailer tailer(fcs);
        Frame frame(header, payload, tailer);

        PacketInfo pkt;
        pkt.seqNum = seqNum;
        pkt.frameObj = frame;
        pkt.serializedData = frame.serialize();
        pkt.transmitCount = 0;
        pkt.isAcked = false;

        packets.push_back(pkt);
        seqNum++;
    }

    int totalPackets = (int)packets.size();
    cout << "\n[SENDER] Partitioned data into " << totalPackets << " frames.\n";
    cout << "Starting transmission using "
         << (protocol == 1 ? "Stop-and-Wait" : (protocol == 2 ? "Go-Back-N" : "Selective Repeat"))
         << " (Window N = " << windowSize << ")...\n\n";

    long long totalTransmissions = 0;
    long long retransmissions = 0;
    long long successfulAcks = 0;
    long long corruptedAcks = 0;

    auto startTime = high_resolution_clock::now();

    // ==========================================
    // PROTOCOL TRANSMISSION ENGINES
    // ==========================================

    const int MAX_RETRIES = 10;

    if (protocol == 1) // STOP-AND-WAIT ARQ
    {
        for (int i = 0; i < totalPackets; i++)
        {
            bool frameAcked = false;
            while (!frameAcked)
            {
                if (packets[i].transmitCount >= MAX_RETRIES)
                {
                    cout << "  [SENDER] Frame " << i << " reached MAX_RETRIES limit (" << MAX_RETRIES << ")! Skipping frame.\n";
                    frameAcked = true;
                    break;
                }

                packets[i].transmitCount++;
                totalTransmissions++;
                if (packets[i].transmitCount > 1) retransmissions++;

                cout << "[SENDER] Sending Frame " << i << " (Attempt " << packets[i].transmitCount
                     << ", Timeout: " << fixed << setprecision(1) << rttEstimator.getTimeoutMs() << " ms)\n";

                packets[i].sendTime = high_resolution_clock::now();

                // Pass frame through channel
                auto transRes = channel.passThrough(packets[i].serializedData, false);
                if (!transRes.isDropped)
                {
                    sendUdp(sendSock, transRes.transmittedData, destAddr);
                }

                // Wait for ACK or Timeout
                bool timedOut = false;
                while (!timedOut && !frameAcked)
                {
                    auto now = high_resolution_clock::now();
                    double elapsedMs = duration_cast<milliseconds>(now - packets[i].sendTime).count();
                    if (elapsedMs >= rttEstimator.getTimeoutMs())
                    {
                        cout << "  [SENDER] TIMEOUT on Frame " << i << " (" << elapsedMs << " ms >= RTO "
                             << rttEstimator.getTimeoutMs() << " ms)!\n";
                        rttEstimator.onTimeout();
                        timedOut = true;
                        break;
                    }

                    string ackMsg;
                    sockaddr_in from{};
                    if (recvUdp(sendSock, ackMsg, from))
                    {
                        AckFrame ack;
                        if (AckFrame::deserialize(ackMsg, ack, fcsMethod, crcChoice))
                        {
                            if (ack.ackSeq == i && !ack.isNack)
                            {
                                auto ackTime = high_resolution_clock::now();
                                double sampleRtt = duration_cast<milliseconds>(ackTime - packets[i].sendTime).count();
                                rttEstimator.updateSampleRTT(sampleRtt, packets[i].transmitCount > 1);

                                cout << "  [SENDER] Valid ACK " << ack.ackSeq << " received (Sample RTT: "
                                     << sampleRtt << " ms, New RTO: " << rttEstimator.getTimeoutMs() << " ms)\n";

                                frameAcked = true;
                                packets[i].isAcked = true;
                                successfulAcks++;
                                break;
                            }
                        }
                        else
                        {
                            cout << "  [SENDER] Corrupted / Invalid ACK received (discarded).\n";
                            corruptedAcks++;
                        }
                    }

                    sleepMs(2);
                }
            }
        }
    }
    else if (protocol == 2) // GO-BACK-N ARQ
    {
        int base = 0;
        int nextSeq = 0;

        while (base < totalPackets)
        {
            // Send packets within window [base, base + windowSize - 1]
            while (nextSeq < base + windowSize && nextSeq < totalPackets)
            {
                packets[nextSeq].transmitCount++;
                totalTransmissions++;
                if (packets[nextSeq].transmitCount > 1) retransmissions++;

                cout << "[SENDER] Sending Frame " << nextSeq << " (Attempt " << packets[nextSeq].transmitCount << ")\n";
                packets[nextSeq].sendTime = high_resolution_clock::now();

                auto transRes = channel.passThrough(packets[nextSeq].serializedData, false);
                if (!transRes.isDropped)
                {
                    sendUdp(sendSock, transRes.transmittedData, destAddr);
                }

                nextSeq++;
            }

            // Check for ACKs
            string ackMsg;
            sockaddr_in from{};
            while (recvUdp(sendSock, ackMsg, from))
            {
                AckFrame ack;
                if (AckFrame::deserialize(ackMsg, ack, fcsMethod, crcChoice))
                {
                    cout << "  [SENDER] Received Cumulative ACK " << ack.ackSeq << "\n";//cumulative ack
                    if (ack.ackSeq >= base)
                    {
                        auto ackTime = high_resolution_clock::now();
                        double sampleRtt = duration_cast<milliseconds>(ackTime - packets[ack.ackSeq].sendTime).count();
                        rttEstimator.updateSampleRTT(sampleRtt, packets[ack.ackSeq].transmitCount > 1);

                        base = ack.ackSeq + 1;
                        successfulAcks++;
                    }
                }
                else
                {
                    cout << "  [SENDER] Corrupted ACK received.\n";
                    corruptedAcks++;
                }
            }

            // Check timeout for oldest unacknowledged packet (base)
            if (base < totalPackets)
            {
                if (packets[base].transmitCount >= MAX_RETRIES)
                {
                    cout << "  [SENDER] Base Frame " << base << " reached MAX_RETRIES (" << MAX_RETRIES << ")! Advancing window.\n";
                    base++;
                    nextSeq = max(nextSeq, base);
                }
                else
                {
                    auto now = high_resolution_clock::now();
                    double elapsedMs = duration_cast<milliseconds>(now - packets[base].sendTime).count();
                    if (elapsedMs >= rttEstimator.getTimeoutMs())
                    {
                        cout << "  [SENDER] GBN TIMEOUT on oldest unACKed Frame " << base << " (" << elapsedMs
                             << " ms >= RTO " << rttEstimator.getTimeoutMs() << " ms) -> Retransmitting window ["
                             << base << " to " << (nextSeq - 1) << "]!\n";

                        rttEstimator.onTimeout();
                        nextSeq = base; // Reset nextSeq to base to retransmit the window
                    }
                }
            }

            sleepMs(5);
        }
    }
    else if (protocol == 3) // SELECTIVE REPEAT ARQ
    {
        int base = 0;
        int nextSeq = 0;

        while (base < totalPackets)
        {
            // Send packets within window
            while (nextSeq < base + windowSize && nextSeq < totalPackets)
            {
                if (!packets[nextSeq].isAcked)
                {
                    packets[nextSeq].transmitCount++;
                    totalTransmissions++;
                    if (packets[nextSeq].transmitCount > 1) retransmissions++;

                    cout << "[SENDER] Sending Frame " << nextSeq << " (Attempt " << packets[nextSeq].transmitCount << ")\n";
                    packets[nextSeq].sendTime = high_resolution_clock::now();

                    auto transRes = channel.passThrough(packets[nextSeq].serializedData, false);
                    if (!transRes.isDropped)
                    {
                        sendUdp(sendSock, transRes.transmittedData, destAddr);
                    }
                }
                nextSeq++;
            }

            // Process ACKs
            string ackMsg;
            sockaddr_in from{};
            while (recvUdp(sendSock, ackMsg, from))
            {
                AckFrame ack;
                if (AckFrame::deserialize(ackMsg, ack, fcsMethod, crcChoice))
                {
                    if (ack.ackSeq >= 0 && ack.ackSeq < totalPackets)
                    {
                        if (!packets[ack.ackSeq].isAcked)
                        {
                            packets[ack.ackSeq].isAcked = true;
                            auto ackTime = high_resolution_clock::now();
                            double sampleRtt = duration_cast<milliseconds>(ackTime - packets[ack.ackSeq].sendTime).count();
                            rttEstimator.updateSampleRTT(sampleRtt, packets[ack.ackSeq].transmitCount > 1);

                            cout << "  [SENDER] Received Individual ACK for Frame " << ack.ackSeq
                                 << " (Sample RTT: " << sampleRtt << " ms)\n";
                            successfulAcks++;
                        }
                    }

                    // Advance base if base is ACKed
                    while (base < totalPackets && packets[base].isAcked)
                    {
                        cout << "  [SENDER] Base packet " << base << " ACKed, advancing sender window to " << (base + 1) << ".\n";
                        base++;
                    }
                }
                else
                {
                    cout << "  [SENDER] Corrupted ACK received.\n";
                    corruptedAcks++;
                }
            }

            // Check individual timeouts for all active in-flight frames in window
            auto now = high_resolution_clock::now();
            for (int i = base; i < nextSeq && i < totalPackets; i++)
            {
                if (!packets[i].isAcked)
                {
                    if (packets[i].transmitCount >= MAX_RETRIES)
                    {
                        cout << "  [SENDER] Frame " << i << " reached MAX_RETRIES (" << MAX_RETRIES << ")! Skipping frame.\n";
                        packets[i].isAcked = true;
                        while (base < totalPackets && packets[base].isAcked) base++;
                        continue;
                    }

                    double elapsedMs = duration_cast<milliseconds>(now - packets[i].sendTime).count();
                    if (elapsedMs >= rttEstimator.getTimeoutMs())
                    {
                        cout << "  [SENDER] SR TIMEOUT on Frame " << i << " (" << elapsedMs << " ms >= RTO "
                             << rttEstimator.getTimeoutMs() << " ms) -> SELECTIVELY Retransmitting Frame " << i << "!\n";

                        rttEstimator.onTimeout();
                        packets[i].transmitCount++;
                        totalTransmissions++;
                        retransmissions++;
                        packets[i].sendTime = high_resolution_clock::now();

                        auto transRes = channel.passThrough(packets[i].serializedData, false);
                        if (!transRes.isDropped)
                        {
                            sendUdp(sendSock, transRes.transmittedData, destAddr);
                        }
                    }
                }
            }

            sleepMs(5);
        }
    }

    // Send EOT (End of Transmission) signal with retry
    for (int t = 0; t < 3; t++)
    {
        sendUdp(sendSock, "CONTROL_EOT", destAddr);
        sleepMs(20);
    }

    auto endTime = high_resolution_clock::now();
    double totalTimeMs = duration_cast<milliseconds>(endTime - startTime).count();
    double throughputBps = (totalTimeMs > 0) ? ((fileContent.length() * 8.0) / (totalTimeMs / 1000.0)) : 0.0;
    double efficiency = (totalTransmissions > 0) ? ((double)totalPackets / totalTransmissions) * 100.0 : 0.0;

    cout << "\n====================================================\n";
    cout << "             TRANSMISSION PERFORMANCE SUMMARY       \n";
    cout << "====================================================\n";
    cout << "Total Data Size       : " << fileContent.length() << " bytes\n";
    cout << "Total Distinct Frames : " << totalPackets << endl;
    cout << "Total Transmissions   : " << totalTransmissions << endl;
    cout << "Retransmissions       : " << retransmissions << endl;
    cout << "Corrupted ACKs Seen   : " << corruptedAcks << endl;
    cout << "Transmission Time     : " << fixed << setprecision(2) << totalTimeMs << " ms\n";
    cout << "Throughput            : " << fixed << setprecision(2) << throughputBps << " bps ("
         << (throughputBps / 8000.0) << " KB/s)\n";
    cout << "Protocol Efficiency   : " << fixed << setprecision(2) << efficiency << " %\n";
    cout << "Final Estimated RTT   : " << fixed << setprecision(2) << rttEstimator.getEstimatedRtt() << " ms\n";
    cout << "Final Timeout (RTO)   : " << fixed << setprecision(2) << rttEstimator.getTimeoutMs() << " ms\n";
    cout << "====================================================\n";

    closeSocket(sendSock);
    cleanupSocketSubsystem();
    return 0;
}
