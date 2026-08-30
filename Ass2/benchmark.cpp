#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <iomanip>
#include <map>

#include "frame.h"
#include "crc.h"
#include "checkSum.h"
#include "channel.h"
#include "rttEstimator.h"

using namespace std;
using namespace chrono;

struct SimStats {
    string protocolName;
    int windowSize;
    double errorProb;
    double lossProb;
    int originalFrames;
    long long totalTransmissions;
    long long retransmissions;
    double totalTimeMs;
    double throughputKbps;
    double efficiencyPercent;
    double avgPropagationDelayMs;
    double avgSampleRttMs;
};

// Simulation engine for in-memory or socket-level protocol benchmarking
SimStats runProtocolSimulation(int protocol, int windowSize, double lossProb, double errProb,
                                int totalFrames, int payloadSize, int fcsMethod, int crcChoice)
{
    Channel dataChannel(ChannelConfig(lossProb, errProb, 1, 10, 25, false));
    Channel ackChannel(ChannelConfig(lossProb, errProb, 1, 10, 25, false));
    RttEstimator rtt(200.0, 30.0, 3000.0);

    string srcMAC = macToBinary("00:1A:2B:3C:4D:5E");
    string destMAC = macToBinary("00:5E:4D:3C:2B:1A");

    // Generate dummy payload data
    vector<string> rawFrames;
    for (int i = 0; i < totalFrames; i++)
    {
        string dummyData(payloadSize, 'A' + (i % 26));
        string dataBin = textToBinary(dummyData);
        string lenBin = uint16ToBinary((unsigned short)payloadSize);
        string seqBin = byteToBinary((unsigned char)(i % 256));

        Header header(srcMAC, destMAC, lenBin, seqBin);
        Payload payload(dataBin);
        string hAndP = header.srcMAC + header.destMAC + header.lengthField + header.seqField + payload.data;

        string fcs = (fcsMethod == 1) ? generateChecksum(hAndP) : generateCRC(hAndP, getCRCGenerator(crcChoice));
        Tailer tailer(fcs);
        Frame frame(header, payload, tailer);
        rawFrames.push_back(frame.serialize());
    }

    int fcsBits = (fcsMethod == 1) ? 16 : (crcChoice == 4 ? 32 : (crcChoice == 3 ? 16 : 8));

    long long totalTransmissions = 0;
    long long retransmissions = 0;
    vector<double> rttSamples;
    vector<double> propDelays;

    auto startTime = high_resolution_clock::now();

    const int MAX_RETRIES = 10;

    if (protocol == 1) // STOP-AND-WAIT
    {
        for (int i = 0; i < totalFrames; i++)
        {
            bool acked = false;
            int attempts = 0;
            while (!acked)
            {
                if (attempts >= MAX_RETRIES) {
                    acked = true;
                    break;
                }

                attempts++;
                totalTransmissions++;
                if (attempts > 1) retransmissions++;

                auto sendT = high_resolution_clock::now();
                auto fRes = dataChannel.passThrough(rawFrames[i], false);
                propDelays.push_back(fRes.delayMs);

                bool frameOk = false;
                if (!fRes.isDropped)
                {
                    Frame rxFrame;
                    if (Frame::deserialize(fRes.transmittedData, fcsBits, rxFrame))
                    {
                        string chkData = rxFrame.getHeaderAndPayload();
                        if (fcsMethod == 1) {
                            frameOk = verifyChecksum(chkData, rxFrame.tailer.fcs);
                        } else {
                            frameOk = verifyCRC(chkData + rxFrame.tailer.fcs, getCRCGenerator(crcChoice));
                        }
                    }
                }

                if (frameOk)
                {
                    // Receiver generates ACK
                    AckFrame ack(i, false);
                    string ackSerialized = ack.serialize(fcsMethod, crcChoice);
                    auto ackRes = ackChannel.passThrough(ackSerialized, true);

                    if (!ackRes.isDropped)
                    {
                        AckFrame rxAck;
                        if (AckFrame::deserialize(ackRes.transmittedData, rxAck, fcsMethod, crcChoice))
                        {
                            if (rxAck.ackSeq == i)
                            {
                                acked = true;
                                auto ackT = high_resolution_clock::now();
                                double sampleRtt = duration_cast<milliseconds>(ackT - sendT).count();
                                rtt.updateSampleRTT(sampleRtt, attempts > 1);
                                rttSamples.push_back(sampleRtt);
                                break;
                            }
                        }
                    }
                }

                if (!acked)
                {
                    // Sleep simulation for timeout
                    sleepMs((int)min(100.0, rtt.getTimeoutMs()));
                    rtt.onTimeout();
                }
            }
        }
    }
    else if (protocol == 2) // GO-BACK-N
    {
        int base = 0;
        int nextSeq = 0;
        int rxExpected = 0;
        vector<int> transmitCounts(totalFrames, 0);

        while (base < totalFrames)
        {
            if (transmitCounts[base] >= MAX_RETRIES)
            {
                base++;
                nextSeq = max(nextSeq, base);
                continue;
            }

            while (nextSeq < base + windowSize && nextSeq < totalFrames)
            {
                transmitCounts[nextSeq]++;
                totalTransmissions++;
                if (transmitCounts[nextSeq] > 1) retransmissions++;

                auto sendT = high_resolution_clock::now();
                auto fRes = dataChannel.passThrough(rawFrames[nextSeq], false);
                propDelays.push_back(fRes.delayMs);

                if (!fRes.isDropped)
                {
                    Frame rxFrame;
                    if (Frame::deserialize(fRes.transmittedData, fcsBits, rxFrame))
                    {
                        string chkData = rxFrame.getHeaderAndPayload();
                        bool valid = (fcsMethod == 1) ? verifyChecksum(chkData, rxFrame.tailer.fcs)
                                                      : verifyCRC(chkData + rxFrame.tailer.fcs, getCRCGenerator(crcChoice));
                        if (valid)
                        {
                            int seq = rxFrame.header.getSequenceNumber();
                            if (seq == rxExpected)
                            {
                                rxExpected++;
                                // Cumulative ACK
                                AckFrame ack(seq, false);
                                string ackStr = ack.serialize(fcsMethod, crcChoice);
                                auto ackRes = ackChannel.passThrough(ackStr, true);
                                if (!ackRes.isDropped)
                                {
                                    AckFrame rxAck;
                                    if (AckFrame::deserialize(ackRes.transmittedData, rxAck, fcsMethod, crcChoice))
                                    {
                                        if (rxAck.ackSeq >= base)
                                        {
                                            auto ackT = high_resolution_clock::now();
                                            double sRtt = duration_cast<milliseconds>(ackT - sendT).count();
                                            rtt.updateSampleRTT(sRtt, transmitCounts[seq] > 1);
                                            rttSamples.push_back(sRtt);
                                            base = rxAck.ackSeq + 1;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                nextSeq++;
            }

            // Timeout check
            if (base < nextSeq && base < totalFrames)
            {
                sleepMs(20);
                rtt.onTimeout();
                nextSeq = base; // Retransmit entire window
            }
        }
    }
    else if (protocol == 3) // SELECTIVE REPEAT
    {
        int base = 0;
        int nextSeq = 0;
        int rxBase = 0;
        vector<bool> senderAcked(totalFrames, false);
        vector<int> transmitCounts(totalFrames, 0);
        map<int, bool> rxBuffer;

        while (base < totalFrames)
        {
            while (nextSeq < base + windowSize && nextSeq < totalFrames)
            {
                if (!senderAcked[nextSeq])
                {
                    transmitCounts[nextSeq]++;
                    totalTransmissions++;
                    if (transmitCounts[nextSeq] > 1) retransmissions++;

                    auto sendT = high_resolution_clock::now();
                    auto fRes = dataChannel.passThrough(rawFrames[nextSeq], false);
                    propDelays.push_back(fRes.delayMs);

                    if (!fRes.isDropped)
                    {
                        Frame rxFrame;
                        if (Frame::deserialize(fRes.transmittedData, fcsBits, rxFrame))
                        {
                            string chkData = rxFrame.getHeaderAndPayload();
                            bool valid = (fcsMethod == 1) ? verifyChecksum(chkData, rxFrame.tailer.fcs)
                                                          : verifyCRC(chkData + rxFrame.tailer.fcs, getCRCGenerator(crcChoice));
                            if (valid)
                            {
                                int seq = rxFrame.header.getSequenceNumber();
                                if (seq >= rxBase && seq < rxBase + windowSize)
                                {
                                    rxBuffer[seq] = true;
                                    while (rxBuffer[rxBase]) { rxBase++; }

                                    AckFrame ack(seq, false);
                                    string ackStr = ack.serialize(fcsMethod, crcChoice);
                                    auto ackRes = ackChannel.passThrough(ackStr, true);
                                    if (!ackRes.isDropped)
                                    {
                                        AckFrame rxAck;
                                        if (AckFrame::deserialize(ackRes.transmittedData, rxAck, fcsMethod, crcChoice))
                                        {
                                            if (rxAck.ackSeq >= 0 && rxAck.ackSeq < totalFrames)
                                            {
                                                senderAcked[rxAck.ackSeq] = true;
                                                auto ackT = high_resolution_clock::now();
                                                double sRtt = duration_cast<milliseconds>(ackT - sendT).count();
                                                rtt.updateSampleRTT(sRtt, transmitCounts[seq] > 1);
                                                rttSamples.push_back(sRtt);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                nextSeq++;
            }

            while (base < totalFrames && senderAcked[base])
            {
                base++;
            }

            if (base < totalFrames)
            {
                // Selective retransmission on timeout
                for (int i = base; i < nextSeq && i < totalFrames; i++)
                {
                    if (!senderAcked[i])
                    {
                        if (transmitCounts[i] >= MAX_RETRIES)
                        {
                            senderAcked[i] = true;
                            while (base < totalFrames && senderAcked[base]) base++;
                            continue;
                        }

                        transmitCounts[i]++;
                        totalTransmissions++;
                        retransmissions++;
                        auto fRes = dataChannel.passThrough(rawFrames[i], false);
                        propDelays.push_back(fRes.delayMs);
                        if (!fRes.isDropped)
                        {
                            Frame rxFrame;
                            if (Frame::deserialize(fRes.transmittedData, fcsBits, rxFrame))
                            {
                                string chkData = rxFrame.getHeaderAndPayload();
                                bool valid = (fcsMethod == 1) ? verifyChecksum(chkData, rxFrame.tailer.fcs)
                                                              : verifyCRC(chkData + rxFrame.tailer.fcs, getCRCGenerator(crcChoice));
                                if (valid)
                                {
                                    int seq = rxFrame.header.getSequenceNumber();
                                    AckFrame ack(seq, false);
                                    string ackStr = ack.serialize(fcsMethod, crcChoice);
                                    auto ackRes = ackChannel.passThrough(ackStr, true);
                                    if (!ackRes.isDropped)
                                    {
                                        senderAcked[seq] = true;
                                    }
                                }
                            }
                        }
                    }
                }
                while (base < totalFrames && senderAcked[base]) base++;
            }
        }
    }

    auto endTime = high_resolution_clock::now();
    double totalTimeMs = duration_cast<milliseconds>(endTime - startTime).count();
    if (totalTimeMs <= 0) totalTimeMs = 1.0;

    double totalDataBytes = totalFrames * payloadSize;
    double throughputKbps = (totalDataBytes * 8.0) / totalTimeMs; // (bits / ms) = kbps
    double efficiency = (totalTransmissions > 0) ? ((double)totalFrames / totalTransmissions) * 100.0 : 0.0;

    double avgProp = 0.0;
    if (!propDelays.empty()) {
        for (double d : propDelays) avgProp += d;
        avgProp /= propDelays.size();
    }

    double avgRtt = 0.0;
    if (!rttSamples.empty()) {
        for (double r : rttSamples) avgRtt += r;
        avgRtt /= rttSamples.size();
    } else {
        avgRtt = avgProp * 2.0;
    }

    SimStats stats;
    stats.protocolName = (protocol == 1 ? "Stop-and-Wait" : (protocol == 2 ? "Go-Back-N" : "Selective-Repeat"));
    stats.windowSize = windowSize;
    stats.errorProb = errProb;
    stats.lossProb = lossProb;
    stats.originalFrames = totalFrames;
    stats.totalTransmissions = totalTransmissions;
    stats.retransmissions = retransmissions;
    stats.totalTimeMs = totalTimeMs;
    stats.throughputKbps = throughputKbps;
    stats.efficiencyPercent = efficiency;
    stats.avgPropagationDelayMs = avgProp;
    stats.avgSampleRttMs = avgRtt;

    return stats;
}

int main()
{
    cout << "====================================================\n";
    cout << "  FLOW CONTROL PERFORMANCE BENCHMARK (SW, GBN, SR)  \n";
    cout << "====================================================\n";

    vector<double> errorRates = {0.0, 0.1, 0.2, 0.3, 0.4, 0.5};
    int totalFrames = 50;
    int payloadSize = 100; // 100 bytes per frame
    int fcsMethod = 2; // CRC
    int crcChoice = 4; // CRC-32

    vector<SimStats> results;

    cout << "Running comprehensive simulation tests across error/loss rates [0.0 to 0.5]...\n\n";

    for (double prob : errorRates)
    {
        cout << "Testing Error/Loss Probability = " << fixed << setprecision(1) << prob << " ...\n";

        // Stop-and-Wait (N=1)
        SimStats swStats = runProtocolSimulation(1, 1, prob, prob, totalFrames, payloadSize, fcsMethod, crcChoice);
        results.push_back(swStats);

        // Go-Back-N (N=4)
        SimStats gbn4Stats = runProtocolSimulation(2, 4, prob, prob, totalFrames, payloadSize, fcsMethod, crcChoice);
        results.push_back(gbn4Stats);

        // Go-Back-N (N=8)
        SimStats gbn8Stats = runProtocolSimulation(2, 8, prob, prob, totalFrames, payloadSize, fcsMethod, crcChoice);
        results.push_back(gbn8Stats);

        // Selective Repeat (N=4)
        SimStats sr4Stats = runProtocolSimulation(3, 4, prob, prob, totalFrames, payloadSize, fcsMethod, crcChoice);
        results.push_back(sr4Stats);

        // Selective Repeat (N=8)
        SimStats sr8Stats = runProtocolSimulation(3, 8, prob, prob, totalFrames, payloadSize, fcsMethod, crcChoice);
        results.push_back(sr8Stats);
    }

    // Save results to CSV
    ofstream csvFile("evaluation_results.csv");
    csvFile << "Protocol,WindowSize,ErrorProb,LossProb,Frames,TotalTx,Retransmissions,TimeMs,ThroughputKbps,EfficiencyPercent,AvgPropDelayMs,AvgRttMs\n";

    cout << "\n========================================================================================================================\n";
    cout << setw(18) << "Protocol"
         << setw(6) << "N"
         << setw(10) << "ErrRate"
         << setw(10) << "TotalTx"
         << setw(10) << "Retx"
         << setw(14) << "Time(ms)"
         << setw(18) << "Throughput(Kbps)"
         << setw(16) << "Efficiency(%)"
         << setw(14) << "AvgRTT(ms)" << "\n";
    cout << "------------------------------------------------------------------------------------------------------------------------\n";

    for (const auto &s : results)
    {
        csvFile << s.protocolName << ","
                << s.windowSize << ","
                << s.errorProb << ","
                << s.lossProb << ","
                << s.originalFrames << ","
                << s.totalTransmissions << ","
                << s.retransmissions << ","
                << fixed << setprecision(2) << s.totalTimeMs << ","
                << s.throughputKbps << ","
                << s.efficiencyPercent << ","
                << s.avgPropagationDelayMs << ","
                << s.avgSampleRttMs << "\n";

        cout << setw(18) << s.protocolName
             << setw(6) << s.windowSize
             << setw(10) << fixed << setprecision(1) << s.errorProb
             << setw(10) << s.totalTransmissions
             << setw(10) << s.retransmissions
             << setw(14) << fixed << setprecision(1) << s.totalTimeMs
             << setw(18) << fixed << setprecision(2) << s.throughputKbps
             << setw(16) << fixed << setprecision(2) << s.efficiencyPercent
             << setw(14) << fixed << setprecision(2) << s.avgSampleRttMs << "\n";
    }
    csvFile.close();

    cout << "========================================================================================================================\n";
    cout << "\nBenchmark complete. Results saved to 'evaluation_results.csv'.\n";
    return 0;
}
