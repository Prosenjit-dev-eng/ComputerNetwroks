/**
 * ============================================================================
 * Assignment 4: CDMA Multi-Station Network Environment Simulation
 * File: simulation.cpp
 * 
 * Description:
 *   All-in-One Multi-Threaded CDMA Simulation (Thread Architecture as in Assignment 3):
 * 
 *   1. Background Channel Medium Thread (`channelServerThreadFunc`):
 *      - Emulates the physical shared channel bus in a dedicated Win32 thread.
 *      - Collects simultaneous chip transmissions from all active stations.
 *      - Linearly superimposes all signals: C_composite[t] = SUM(S_i[t]).
 *      - Simulates AWGN channel noise (if enabled).
 *      - Broadcasts the composite signal to all station listener threads.
 * 
 *   2. For-Loop Station TX Threads (`stationTxThreadFunc`):
 *      - Spawns n concurrent station transmitter threads via CreateThread().
 *      - Encapsulates messages into IEEE 802 standard frames with 32-bit CRC.
 *      - Spreads frames into chip sequences using unique Walsh codes C_i.
 *      - Transmits chips concurrently to the shared medium.
 * 
 *   3. Concurrent Background Station RX Listener Threads (`stationRxListenerThread`):
 *      - Each station runs a background listener thread (Full-Duplex Dual-Thread).
 *      - Concurrently receives composite broadcasts from the channel.
 *      - Despreads using normalized inner products: (C_composite . C_j) / N.
 *      - Verifies CRC-32 Frame Check Sequence (FCS) in the background.
 *      - Signals the TX thread upon completion via Win32 Events.
 * 
 * Compilation (MinGW / GCC on Windows):
 *   g++ -O2 -std=c++11 simulation.cpp cdma.cpp frame.cpp socketCommon.cpp -o simulation.exe -lws2_32
 * 
 * Execution:
 *   simulation.exe [num_stations] [demo_mode: 1=Full Frames, 2=Step-by-Step Math, 3=Noise Stress, 4=Silent Station Test] [noise_snr_dB]
 * ============================================================================
 */

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <atomic>
#include <cmath>
#include <algorithm>

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "cdma.h"
#include "frame.h"
#include "socketCommon.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;
using namespace std::chrono;

// Thread-safe console printing
CRITICAL_SECTION g_console_cs;

void printLog(const string &msg) {
    EnterCriticalSection(&g_console_cs);
    cout << msg << flush;
    LeaveCriticalSection(&g_console_cs);
}

// Forward declarations
struct SimulationChannel;

// Structure for each Station's state and dual threads
struct StationNode {
    int stationID;
    int totalStations;
    int walshOrder;
    vector<int> myWalshCode;
    vector<vector<int>> allWalshCodes;
    string inputMessage;
    bool isSilent;

    // Transmitted data
    string transmittedBits;
    vector<int> transmittedChips;

    // Received data (populated by RX Listener Thread)
    string reconstructedBits;
    string reconstructedMessage;
    bool crcPassed;
    int bitErrors;

    // Synchronization events
    HANDLE hStartTxEvent;
    HANDLE hBroadcastReadyEvent;
    HANDLE hRxDoneEvent;
    std::atomic<bool> stopListener{false};

    // Thread handles
    HANDLE hTxThread;
    HANDLE hRxThread;

    SimulationChannel* channel;
};

// Global shared channel state emulating physical medium
struct SimulationChannel {
    int numStations;
    int walshOrder;
    vector<vector<int>> walshMatrix;
    vector<vector<int>> stationChips;
    vector<int> compositeSignal;
    vector<double> noisyCompositeSignal;
    double snr_dB;
    bool noiseEnabled;

    HANDLE hAllStationsDepositedEvent;
    HANDLE hSuperpositionCompleteEvent;
    HANDLE hChannelDoneEvent;

    std::atomic<int> activeTransmittersCount{0};
    std::atomic<int> completedRxCount{0};
    std::atomic<bool> channelRunning{true};

    HANDLE hServerThread;
    vector<StationNode*> stations;
};

// ============================================================================
// 1. BACKGROUND STATION RX LISTENER THREAD
// ============================================================================
DWORD WINAPI stationRxListenerThread(LPVOID lpParam) {
    StationNode* st = (StationNode*)lpParam;
    SimulationChannel* ch = st->channel;

    // Wait until the channel server thread finishes superposition and broadcasts
    WaitForSingleObject(st->hBroadcastReadyEvent, INFINITE);
    if (st->stopListener.load()) return 0;

    if (!st->isSilent) {
        if (ch->noiseEnabled) {
            st->reconstructedBits = decodeNoisySignal(ch->noisyCompositeSignal, st->myWalshCode, (int)st->transmittedBits.length());
        } else {
            st->reconstructedBits = decodeCompositeSignal(ch->compositeSignal, st->myWalshCode, (int)st->transmittedBits.length());
        }

        // Measure bit errors
        st->bitErrors = 0;
        for (size_t b = 0; b < st->transmittedBits.length(); b++) {
            if (b < st->reconstructedBits.length() && st->transmittedBits[b] != st->reconstructedBits[b]) {
                st->bitErrors++;
            }
        }

        Frame rxFrame;
        bool ok = Frame::deserialize(st->reconstructedBits, FrameConstants::CRC32_BITS, rxFrame);
        if (ok && verifyCRC32(rxFrame.getHeaderAndPayload() + rxFrame.tailer.fcs)) {
            st->crcPassed = true;
            string text = binaryToText(rxFrame.payload.data);
            size_t nullPos = text.find('\0');
            if (nullPos != string::npos) text = text.substr(0, nullPos);
            st->reconstructedMessage = text;
        } else {
            st->crcPassed = false;
            st->reconstructedMessage = "[DECODE / CRC FAILED]";
        }
    } else {
        st->crcPassed = true;
        st->reconstructedMessage = "[STATION WAS SILENT]";
        st->bitErrors = 0;
    }

    // Signal completion
    ch->completedRxCount++;
    SetEvent(st->hRxDoneEvent);
    return 0;
}

// ============================================================================
// 2. CONCURRENT STATION TX THREAD
// ============================================================================
DWORD WINAPI stationTxThreadFunc(LPVOID lpParam) {
    StationNode* st = (StationNode*)lpParam;
    SimulationChannel* ch = st->channel;

    // Wait for channel start signal
    WaitForSingleObject(st->hStartTxEvent, INFINITE);

    if (st->isSilent) {
        st->transmittedBits = "";
        st->transmittedChips.assign(ch->walshOrder * 8, 0); // 0 volts / idle
    } else {
        // Build IEEE 802 frame
        stringstream srcMacSS, dstMacSS;
        srcMacSS << "00:11:22:33:44:" << setfill('0') << setw(2) << hex << st->stationID;
        dstMacSS << "FF:FF:FF:FF:FF:FF";

        string binaryPayload = textToBinary(st->inputMessage);
        while (binaryPayload.length() < FrameConstants::MIN_PAYLOAD_BYTES * 8) {
            binaryPayload += "0";
        }

        unsigned short payloadBytes = (unsigned short)(binaryPayload.length() / 8);
        string lenBin = uint16ToBinary(payloadBytes);
        string seqBin = byteToBinary((unsigned char)1);

        Header header(macToBinary(srcMacSS.str()), macToBinary(dstMacSS.str()), lenBin, seqBin);
        Payload payload(binaryPayload);
        string headerAndPayload = header.srcMAC + header.destMAC + header.lengthField + header.seqField + payload.data;
        string fcs = computeCRC32(headerAndPayload);
        Tailer tailer(fcs);

        Frame txFrame(header, payload, tailer);
        st->transmittedBits = txFrame.serialize();
        st->transmittedChips = encodeBitstream(st->transmittedBits, st->myWalshCode);
    }

    // Deposit chips into shared medium
    ch->stationChips[st->stationID] = st->transmittedChips;

    // Check if all stations deposited chips
    if (++ch->activeTransmittersCount == ch->numStations) {
        SetEvent(ch->hAllStationsDepositedEvent);
    }

    // Wait for RX listener thread to finish decoding before TX thread exits
    WaitForSingleObject(st->hRxDoneEvent, INFINITE);
    return 0;
}

// ============================================================================
// 3. BACKGROUND SHARED CHANNEL SERVER THREAD
// ============================================================================
DWORD WINAPI channelServerThreadFunc(LPVOID lpParam) {
    SimulationChannel* ch = (SimulationChannel*)lpParam;

    // 1. Wait until all concurrent station TX threads have deposited their chips
    WaitForSingleObject(ch->hAllStationsDepositedEvent, INFINITE);

    // 2. Perform linear superposition (Vector summation of all station chip signals)
    ch->compositeSignal = superimposeSignals(ch->stationChips);

    // 3. Inject noise if enabled
    if (ch->noiseEnabled) {
        ch->noisyCompositeSignal = addChannelNoise(ch->compositeSignal, ch->snr_dB, 2026);
    }

    // 4. Broadcast to all station RX listener threads
    for (int i = 0; i < ch->numStations; i++) {
        SetEvent(ch->stations[i]->hBroadcastReadyEvent);
    }

    SetEvent(ch->hSuperpositionCompleteEvent);
    return 0;
}

// Step-by-step mathematical demonstration
void runStepByStepMathDemo(int numStations) {
    int N = nextPowerOf2(numStations);
    cout << "\n===================================================================\n";
    cout << "   CDMA MATHEMATICAL DEMONSTRATION: STEP-BY-STEP ORTHOGONAL DESPREADING\n";
    cout << "===================================================================\n";

    vector<vector<int>> walsh = generateWalshMatrix(N);
    printWalshMatrix(walsh, cout);
    cout << "\n";

    vector<int> sampleBits(numStations);
    for (int i = 0; i < numStations; i++) {
        sampleBits[i] = (i % 2 == 0) ? 1 : 0;
    }

    cout << "--- [STEP 1: STATION BITS & CHIP SPREADING] ---\n";
    vector<vector<int>> chips(numStations, vector<int>(N, 0));
    for (int i = 0; i < numStations; i++) {
        int bipolar = (sampleBits[i] == 1) ? +1 : -1;
        for (int k = 0; k < N; k++) {
            chips[i][k] = bipolar * walsh[i][k];
        }
        cout << "  Station " << i << ": Bit = " << sampleBits[i] 
             << " (" << (bipolar > 0 ? "+1" : "-1") << ") * Walsh " << walshVectorToString(walsh[i])
             << " = " << walshVectorToString(chips[i]) << "\n";
    }

    cout << "\n--- [STEP 2: COMMON MEDIUM LINEAR SUPERPOSITION (SUM OF CHIPS)] ---\n";
    vector<int> composite(N, 0);
    for (int k = 0; k < N; k++) {
        for (int i = 0; i < numStations; i++) {
            composite[k] += chips[i][k];
        }
    }
    cout << "  Composite Signal C_composite = SUM(S_i for i=0.." << numStations - 1 << "):\n";
    cout << "  " << walshVectorToString(composite) << "\n\n";

    cout << "--- [STEP 3: DESPREADING AT RECEIVERS (DOT PRODUCT WITH WALSH CODES)] ---\n";
    cout << "  Formula: Bit = (C_composite . C_j) / N\n\n";

    for (int j = 0; j < numStations; j++) {
        int dot = 0;
        stringstream dotExpr;
        dotExpr << "(";
        for (int k = 0; k < N; k++) {
            dot += composite[k] * walsh[j][k];
            dotExpr << "(" << composite[k] << "*" << walsh[j][k] << ")";
            if (k + 1 < N) dotExpr << " + ";
        }
        dotExpr << ")";

        int recoveredBipolar = dot / N;
        int recoveredBit = (recoveredBipolar > 0) ? 1 : 0;
        bool match = (recoveredBit == sampleBits[j]);

        cout << "  Receiver " << j << " (using C_" << j << "): " << dotExpr.str() << " = " << dot << "\n";
        cout << "    Normalized Inner Product: " << dot << " / " << N << " = " << recoveredBipolar << "\n";
        cout << "    Decoded Bit: " << recoveredBit 
             << " | Original: " << sampleBits[j] 
             << " --> " << (match ? "[PERFECT MATCH]" : "[MISMATCH]") << "\n\n";
    }
    cout << "===================================================================\n";
}

// ============================================================================
// MAIN SIMULATION ENTRY POINT
// ============================================================================
int main(int argc, char* argv[]) {
    InitializeCriticalSection(&g_console_cs);

    int numStations = 4;
    int demoMode = 1;
    double snr_dB = 25.0;

    if (argc > 1) numStations = atoi(argv[1]);
    if (argc > 2) demoMode = atoi(argv[2]);
    if (argc > 3) snr_dB = atof(argv[3]);

    if (numStations < 2) numStations = 2;
    int walshOrder = nextPowerOf2(numStations);


    cout << "  Number of Stations:  " << numStations << "\n";
    cout << "  Walsh Code Order:    N = " << walshOrder << " (" << walshOrder << " chips per data bit)\n";
    cout << "  Selected Demo Mode:  " << demoMode << " (";
    if (demoMode == 1) cout << "Full IEEE 802 Frame Simulation with CRC-32";
    else if (demoMode == 2) cout << "Step-by-Step Mathematical Matrix Demo";
    else if (demoMode == 3) cout << "Additive Gaussian Noise (AWGN) Stress Test";
    else if (demoMode == 4) cout << "Silent / Asynchronous Station Multi-Access Test";
    cout << ")\n";
    if (demoMode == 3) cout << "  Target SNR:          " << snr_dB << " dB\n";
    cout << "===================================================================\n\n";

    if (demoMode == 2) {
        runStepByStepMathDemo(numStations);
        DeleteCriticalSection(&g_console_cs);
        return 0;
    }

    // 1. Generate Walsh Matrix & Prove Orthogonality
    vector<vector<int>> walsh = generateWalshMatrix(walshOrder);
    printWalshMatrix(walsh, cout);
    cout << "\n";
    verifyWalshOrthogonality(walsh, &cout);
    cout << "\n";

    // 2. Setup Channel Thread Structures
    SimulationChannel channel;
    channel.numStations = numStations;
    channel.walshOrder = walshOrder;
    channel.walshMatrix = walsh;
    channel.stationChips.resize(numStations);
    channel.snr_dB = snr_dB;
    channel.noiseEnabled = (demoMode == 3);

    channel.hAllStationsDepositedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    channel.hSuperpositionCompleteEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    channel.hChannelDoneEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Launch Channel Server in background thread (Win32 CreateThread)
    channel.hServerThread = CreateThread(NULL, 0, channelServerThreadFunc, &channel, 0, NULL);
    cout << "[THREAD SYSTEM] Background Shared Channel Server Thread Launched.\n";

    // 3. Setup and Launch Station Nodes (Each with TX Thread and Background RX Listener Thread)
    vector<StationNode> stationNodes(numStations);
    vector<HANDLE> txThreadHandles(numStations);
    vector<HANDLE> rxThreadHandles(numStations);

    cout << "\n--- [STATION CONFIGURATION & PAYLOADS] ---\n";
    for (int i = 0; i < numStations; i++) {
        StationNode& st = stationNodes[i];
        st.stationID = i;
        st.totalStations = numStations;
        st.walshOrder = walshOrder;
        st.myWalshCode = walsh[i];
        st.allWalshCodes = walsh;
        st.isSilent = (demoMode == 4 && i == numStations - 1);
        st.channel = &channel;

        st.hStartTxEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        st.hBroadcastReadyEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        st.hRxDoneEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        if (st.isSilent) {
            st.inputMessage = "";
            cout << "  Station " << i << " (Walsh C_" << i << "): [IDLE / SILENT STATION - NO TRANSMISSION]\n";
        } else {
            stringstream msgSS;
            msgSS << "Station-" << i << " CDMA Packet: High-speed simultaneous transmission!";
            st.inputMessage = msgSS.str();
            cout << "  Station " << i << " (Walsh C_" << i << "): \"" << st.inputMessage << "\"\n";
        }

        channel.stations.push_back(&st);

        // A. Launch background RX Listener Thread for Station i
        st.hRxThread = CreateThread(NULL, 0, stationRxListenerThread, &st, 0, NULL);
        rxThreadHandles[i] = st.hRxThread;

        // B. Launch concurrent TX Thread for Station i
        st.hTxThread = CreateThread(NULL, 0, stationTxThreadFunc, &st, 0, NULL);
        txThreadHandles[i] = st.hTxThread;
    }
    cout << "------------------------------------------\n";
    cout << "[THREAD SYSTEM] Spawned " << numStations << " Station TX Threads and " 
         << numStations << " Background RX Listener Threads.\n\n";

    auto startTime = high_resolution_clock::now();

    // Trigger all Station TX threads to begin concurrent transmissions
    cout << "[COORDINATOR] Signalling start event to all Station TX threads...\n";
    for (int i = 0; i < numStations; i++) {
        SetEvent(stationNodes[i].hStartTxEvent);
    }

    // Wait for the Channel Server thread to complete superposition and broadcast
    WaitForSingleObject(channel.hSuperpositionCompleteEvent, INFINITE);
    cout << "[CHANNEL THREAD] Superposition Complete. Composite Signal length: " 
         << channel.compositeSignal.size() << " chips.\n";

    if (channel.noiseEnabled) {
        cout << "[CHANNEL THREAD] Injected AWGN Channel Noise (SNR = " << snr_dB << " dB).\n";
    }

    // Wait for all Station TX and RX Listener threads to finish
    WaitForMultipleObjects(numStations, txThreadHandles.data(), TRUE, INFINITE);
    WaitForMultipleObjects(numStations, rxThreadHandles.data(), TRUE, INFINITE);
    WaitForSingleObject(channel.hServerThread, INFINITE);

    auto endTime = high_resolution_clock::now();
    double elapsedMs = duration<double, milli>(endTime - startTime).count();

    // 4. Print Results Table
    cout << "\n=======================================================================================================\n";
    cout << "                                  SIMULATION RESULTS & PERFORMANCE REPORT                              \n";
    cout << "=======================================================================================================\n";
    cout << left << setw(10) << "Station" 
         << setw(16) << "Walsh Code" 
         << setw(12) << "Tx Bits" 
         << setw(12) << "Tx Chips" 
         << setw(12) << "Rx Bits" 
         << setw(12) << "Bit Errors" 
         << setw(14) << "CRC-32 FCS" 
         << "Recovered Payload Status\n";
    cout << string(103, '-') << "\n";

    int totalBits = 0;
    int totalChips = 0;
    int totalErrors = 0;
    int totalFramesPassed = 0;
    int activeStations = 0;

    for (int i = 0; i < numStations; i++) {
        if (!stationNodes[i].isSilent) {
            activeStations++;
            totalBits += (int)stationNodes[i].transmittedBits.length();
            totalChips += (int)stationNodes[i].transmittedChips.size();
            totalErrors += stationNodes[i].bitErrors;
            if (stationNodes[i].crcPassed) totalFramesPassed++;
        }

        stringstream walshSummary;
        walshSummary << "C_" << i << " (" << walshOrder << "c)";

        cout << left << setw(10) << ("Node " + to_string(i))
             << setw(16) << walshSummary.str()
             << setw(12) << (stationNodes[i].isSilent ? 0 : (int)stationNodes[i].transmittedBits.length())
             << setw(12) << (stationNodes[i].isSilent ? 0 : (int)stationNodes[i].transmittedChips.size())
             << setw(12) << (stationNodes[i].isSilent ? 0 : (int)stationNodes[i].reconstructedBits.length())
             << setw(12) << stationNodes[i].bitErrors
             << setw(14) << (stationNodes[i].crcPassed ? "PASS [OK]" : "FAIL")
             << (stationNodes[i].isSilent ? "[STATION IDLE]" : ("\"" + stationNodes[i].reconstructedMessage.substr(0, 30) + "...\""))
             << "\n";
    }
    cout << string(103, '-') << "\n";

    double ber = (totalBits > 0) ? ((double)totalErrors / (double)totalBits) * 100.0 : 0.0;
    double processingGain_dB = 10.0 * log10((double)walshOrder);

    cout << "\n--- [SYSTEM AGGREGATE METRICS] ---\n";
    cout << "  Active Transmitter Threads:  " << activeStations << " / " << numStations << "\n";
    cout << "  Background Listener Threads: " << numStations << "\n";
    cout << "  Dedicated Channel Thread:    1\n";
    cout << "  Total Active System Threads: " << (2 * numStations + 2) << "\n";
    cout << "  Walsh Spreading Factor (N):  " << walshOrder << " chips/bit\n";
    cout << "  Theoretical Processing Gain: " << fixed << setprecision(2) << processingGain_dB << " dB (10*log10(N))\n";
    cout << "  Total Information Bits Sent: " << totalBits << " bits\n";
    cout << "  Total Channel Chips Used:    " << channel.compositeSignal.size() << " chips\n";
    cout << "  Total Bit Errors:            " << totalErrors << " bits\n";
    cout << "  Bit Error Rate (BER):        " << fixed << setprecision(4) << ber << "%\n";
    cout << "  Frame Delivery Success Rate: " << totalFramesPassed << "/" << activeStations 
         << " (" << (activeStations > 0 ? (totalFramesPassed * 100 / activeStations) : 0) << "%)\n";
    cout << "  Execution Wall-Clock Time:   " << fixed << setprecision(2) << elapsedMs << " ms\n";
    cout << "=======================================================================================================\n";

    if (totalErrors == 0 && totalFramesPassed == activeStations) {
        cout << ">>> VERIFICATION PASSED: PERFECT RECONSTRUCTION ACHIEVED ACROSS ALL CONCURRENT THREADS! <<<\n";
    } else {
        cout << ">>> NOTICE: Errors occurred due to simulated channel noise (SNR=" << snr_dB << " dB). <<<\n";
    }
    cout << "=======================================================================================================\n";

    // Clean up handles
    for (int i = 0; i < numStations; i++) {
        CloseHandle(stationNodes[i].hStartTxEvent);
        CloseHandle(stationNodes[i].hBroadcastReadyEvent);
        CloseHandle(stationNodes[i].hRxDoneEvent);
        CloseHandle(txThreadHandles[i]);
        CloseHandle(rxThreadHandles[i]);
    }
    CloseHandle(channel.hAllStationsDepositedEvent);
    CloseHandle(channel.hSuperpositionCompleteEvent);
    CloseHandle(channel.hChannelDoneEvent);
    CloseHandle(channel.hServerThread);

    DeleteCriticalSection(&g_console_cs);
    return 0;
}
