/**
 * ============================================================================
 * Assignment 4: CDMA Network Environment Emulation
 * File: station.cpp
 * 
 * Description:
 *   CDMA Station Node with Concurrent Dual-Thread Architecture (like Assignment 3):
 * 
 *   +-------------------------------------------------------------------+
 *   |                           STATION NODE                            |
 *   |                                 |                                 |
 *   |              +------------------+------------------+              |
 *   |              |                                     |              |
 *   |      TX / Main Thread                     RX / Listener Thread    |
 *   |              |                                     |              |
 *   |     - Encapsulates IEEE 802 Frame        - Continuously calls     |
 *   |     - Computes 32-bit CRC FCS              recv() in background   |
 *   |     - Spreads bits with Walsh C_i        - Parses CONFIG, TX_READY|
 *   |     - Transmits chips to channel         - Receives COMPOSITE     |
 *   |     - Waits for composite event            channel broadcasts     |
 *   |              |                           - Despreads & reconstruct|
 *   |              |                           - Verifies CRC-32 FCS    |
 *   |              +------------------+------------------+              |
 *   |                                 |                                 |
 *   |                 Win32 Synchronization Events                      |
 *   |        (hConfigEvent, hTxReadyEvent, hCompositeEvent)             |
 *   +-------------------------------------------------------------------+
 * 
 * Compilation (MinGW / GCC on Windows):
 *   g++ -O2 -std=c++11 station.cpp cdma.cpp frame.cpp socketCommon.cpp -o station.exe -lws2_32
 * 
 * Execution:
 *   station.exe [station_name] [server_ip] [server_port] [message]
 * ============================================================================
 */

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <atomic>

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "cdma.h"
#include "frame.h"
#include "socketCommon.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// Thread-safe console printing
CRITICAL_SECTION g_console_cs;

void printLog(const string &msg) {
    EnterCriticalSection(&g_console_cs);
    cout << msg << flush;
    LeaveCriticalSection(&g_console_cs);
}

// Station shared context across TX and RX threads
struct StationContext {
    SOCKET sock;
    string stationName;
    int stationID;
    int totalStations;
    int walshOrder;
    vector<int> myWalshCode;
    vector<vector<int>> allWalshCodes;

    std::atomic<bool> stopListener{false};
    HANDLE hConfigEvent;
    HANDLE hTxReadyEvent;
    HANDLE hCompositeEvent;

    CRITICAL_SECTION dataCs;
    vector<int> latestCompositeChips;
    int lastDecodedCount;
    bool connectionClosed;
};

// ============================================================================
// CONCURRENT RX / LISTENER THREAD (Runs asynchronously in the background)
// ============================================================================
DWORD WINAPI stationListenerThread(LPVOID lpParam) {
    StationContext* ctx = (StationContext*)lpParam;
    string stream = "";
    char buffer[4096];

    while (!ctx->stopListener.load()) {
        int bytesRead = recv(ctx->sock, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            ctx->connectionClosed = true;
            SetEvent(ctx->hConfigEvent);
            SetEvent(ctx->hTxReadyEvent);
            SetEvent(ctx->hCompositeEvent);
            break;
        }

        buffer[bytesRead] = '\0';
        stream += buffer;

        size_t newlinePos;
        while ((newlinePos = stream.find('\n')) != string::npos) {
            string line = stream.substr(0, newlinePos);
            stream.erase(0, newlinePos + 1);

            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            stringstream ss(line);
            string tag;
            ss >> tag;

            if (tag == "CONFIG") {
                // CONFIG <stationID> <totalStations> <walshOrder> <myChips...> <allChips...>
                int sid = 0, total = 0, order = 0;
                ss >> sid >> total >> order;

                EnterCriticalSection(&ctx->dataCs);
                ctx->stationID = sid;
                ctx->totalStations = total;
                ctx->walshOrder = order;

                ctx->myWalshCode.resize(order);
                for (int k = 0; k < order; k++) {
                    ss >> ctx->myWalshCode[k];
                }

                ctx->allWalshCodes.assign(total, vector<int>(order));
                for (int s = 0; s < total; s++) {
                    for (int k = 0; k < order; k++) {
                        ss >> ctx->allWalshCodes[s][k];
                    }
                }
                LeaveCriticalSection(&ctx->dataCs);

                SetEvent(ctx->hConfigEvent);
            }
            else if (tag == "TX_READY") {
                SetEvent(ctx->hTxReadyEvent);
            }
            else if (tag == "COMPOSITE") {
                // COMPOSITE <num_chips> <c0> <c1> ...
                int numChips = 0;
                ss >> numChips;

                vector<int> composite(numChips);
                for (int c = 0; c < numChips; c++) {
                    ss >> composite[c];
                }

                EnterCriticalSection(&ctx->dataCs);
                ctx->latestCompositeChips = composite;
                LeaveCriticalSection(&ctx->dataCs);

                // Asynchronously despread and process incoming transmissions in the listener thread
                stringstream logSS;
                logSS << "\n  [RX LISTENER THREAD] Received " << numChips 
                      << " composite chips from shared channel.\n";
                logSS << "  [RX LISTENER THREAD] Despreading & Reconstructing All Stations:\n";
                logSS << "  -----------------------------------------------------------------\n";

                int successCount = 0;
                int total = ctx->totalStations;
                int order = ctx->walshOrder;

                for (int s = 0; s < total; s++) {
                    // Despread Station s's bitstream
                    string reconstructedBits = decodeCompositeSignal(composite, ctx->allWalshCodes[s]);

                    Frame rxFrame;
                    bool ok = Frame::deserialize(reconstructedBits, FrameConstants::CRC32_BITS, rxFrame);
                    bool crcValid = false;
                    string recoveredText = "";

                    if (ok) {
                        crcValid = verifyCRC32(rxFrame.getHeaderAndPayload() + rxFrame.tailer.fcs);
                        recoveredText = binaryToText(rxFrame.payload.data);
                        size_t nullPos = recoveredText.find('\0');
                        if (nullPos != string::npos) recoveredText = recoveredText.substr(0, nullPos);
                    }

                    logSS << "  * Station " << s << " (Walsh C_" << s << "): ";
                    if (crcValid) {
                        logSS << "[CRC-32 PASS] Payload: \"" << recoveredText << "\"\n";
                        successCount++;
                    } else {
                        logSS << "[CRC FAIL / CORRUPTED]\n";
                    }
                }
                logSS << "  -----------------------------------------------------------------\n";
                logSS << "  Listener Reconstruction Accuracy: " << successCount << "/" << total 
                      << " (" << (total > 0 ? (successCount * 100 / total) : 0) << "%)\n\n";

                printLog(logSS.str());

                ctx->lastDecodedCount = successCount;
                SetEvent(ctx->hCompositeEvent);
            }
        }
    }
    return 0;
}

// Helper to send line-delimited message
bool sendTcpLine(SOCKET s, const string &msg) {
    string packet = msg + "\n";
    int total = (int)packet.length();
    int sent = 0;
    while (sent < total) {
        int n = send(s, packet.c_str() + sent, total - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

// ============================================================================
// MAIN / TX THREAD
// ============================================================================
int main(int argc, char* argv[]) {
    InitializeCriticalSection(&g_console_cs);

    if (!initializeSocketSubsystem()) {
        cerr << "[ERROR] Winsock initialization failed.\n";
        DeleteCriticalSection(&g_console_cs);
        return 1;
    }

    string stationName = "Station_0";
    string serverIP = "127.0.0.1";
    int serverPort = 5050;
    string customMessage = "";

    if (argc > 1) stationName = argv[1];
    if (argc > 2) serverIP = argv[2];
    if (argc > 3) serverPort = atoi(argv[3]);
    if (argc > 4) customMessage = argv[4];

    cout << "===================================================================\n";
    cout << "  CDMA STATION NODE: " << stationName << " (Dual-Thread Architecture)\n";
    cout << "===================================================================\n";

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        cerr << "[ERROR] Socket creation failed: " << WSAGetLastError() << "\n";
        cleanupSocketSubsystem();
        DeleteCriticalSection(&g_console_cs);
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    serverAddr.sin_addr.s_addr = inet_addr(serverIP.c_str());

    cout << "[CONNECT] Connecting to CDMA Channel Server at " << serverIP << ":" << serverPort << "...\n";
    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "[ERROR] Connection failed: " << WSAGetLastError() << "\n";
        closeSocket(sock);
        cleanupSocketSubsystem();
        DeleteCriticalSection(&g_console_cs);
        return 1;
    }
    cout << "[SUCCESS] Connected to CDMA Channel Server!\n\n";

    // Setup station context
    StationContext ctx;
    ctx.sock = sock;
    ctx.stationName = stationName;
    ctx.stationID = 0;
    ctx.totalStations = 0;
    ctx.walshOrder = 0;
    ctx.lastDecodedCount = 0;
    ctx.connectionClosed = false;

    InitializeCriticalSection(&ctx.dataCs);
    ctx.hConfigEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    ctx.hTxReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    ctx.hCompositeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    // Launch background RX listener thread (Win32 CreateThread)
    HANDLE hListener = CreateThread(NULL, 0, stationListenerThread, &ctx, 0, NULL);
    if (hListener == NULL) {
        cerr << "[ERROR] Failed to create RX listener thread.\n";
        closeSocket(sock);
        cleanupSocketSubsystem();
        return 1;
    }
    cout << "[THREAD] Concurrent RX Listener Thread successfully spawned.\n";

    int round = 1;

    // Wait for CONFIG handshake from server via listener thread
    WaitForSingleObject(ctx.hConfigEvent, INFINITE);
    if (ctx.connectionClosed) {
        cerr << "[ERROR] Server closed connection during handshake.\n";
        goto cleanup;
    }

    cout << "\n--- [STATION CONFIGURATION RECEIVED BY LISTENER THREAD] ---\n";
    cout << "  Assigned Station ID: " << ctx.stationID << " (of " << ctx.totalStations << " stations)\n";
    cout << "  Walsh Code Order:    N = " << ctx.walshOrder << "\n";
    cout << "  My Walsh Code C_" << ctx.stationID << ":   " << walshVectorToString(ctx.myWalshCode) << "\n";
    cout << "----------------------------------------------------------\n\n";
    while (!ctx.connectionClosed) {
        // Wait for TX_READY trigger from listener thread
        cout << "[TX THREAD] Waiting for channel synchronization signal (TX_READY)...\n";
        WaitForSingleObject(ctx.hTxReadyEvent, INFINITE);
        if (ctx.connectionClosed) break;

        cout << "\n>>> [ROUND " << round << "] TX / Main Thread Preparing Frame <<<\n";

        string messageToSend = customMessage;
        if (messageToSend.empty()) {
            if (argc > 4 && round == 1) {
                messageToSend = customMessage;
            } else {
                stringstream defSS;
                defSS << "Hello from " << stationName << "! [Dual-Thread Packet #" << round << "]";
                messageToSend = defSS.str();
            }
        }

        // Encapsulate into IEEE 802 standard frame with CRC-32
        stringstream srcMacSS, dstMacSS;
        srcMacSS << "00:11:22:33:44:" << setfill('0') << setw(2) << hex << ctx.stationID;
        dstMacSS << "FF:FF:FF:FF:FF:FF";

        string binaryPayload = textToBinary(messageToSend);
        while (binaryPayload.length() < FrameConstants::MIN_PAYLOAD_BYTES * 8) {
            binaryPayload += "0";
        }

        unsigned short payloadBytes = (unsigned short)(binaryPayload.length() / 8);
        string lenBin = uint16ToBinary(payloadBytes);
        string seqBin = byteToBinary((unsigned char)round);

        Header header(macToBinary(srcMacSS.str()), macToBinary(dstMacSS.str()), lenBin, seqBin);
        Payload payload(binaryPayload);
        string headerAndPayload = header.srcMAC + header.destMAC + header.lengthField + header.seqField + payload.data;
        string fcs = computeCRC32(headerAndPayload);
        Tailer tailer(fcs);

        Frame txFrame(header, payload, tailer);
        string serializedFrame = txFrame.serialize();

        cout << "  [FRAME CREATED BY TX THREAD]\n";
        cout << "    Source MAC:       " << srcMacSS.str() << "\n";
        cout << "    Destination MAC:  " << dstMacSS.str() << "\n";
        cout << "    Sequence Number:  " << round << "\n";
        cout << "    Payload:          \"" << messageToSend << "\" (" << payloadBytes << " bytes)\n";
        cout << "    Total Frame Bits: " << serializedFrame.length() << " bits\n";
        cout << "    CRC-32 FCS:       0b" << fcs << "\n";

        // Encode / Spread bits with Walsh codeword
        vector<int> txChips = encodeBitstream(serializedFrame, ctx.myWalshCode);
        cout << "    CDMA Spreading:   " << serializedFrame.length() << " bits * " 
             << ctx.walshOrder << " chips/bit = " << txChips.size() << " chips\n";

        // Transmit chips to channel server
        stringstream chipsSS;
        chipsSS << "CHIPS " << ctx.stationID << " " << txChips.size();
        for (int chip : txChips) {
            chipsSS << " " << chip;
        }
        sendTcpLine(sock, chipsSS.str());
        cout << "  [TX THREAD] Transmitted " << txChips.size() << " chips to shared medium.\n";

        // Wait for background RX listener thread to receive and decode the broadcast composite signal!
        cout << "  [TX THREAD] Yielding; waiting for RX listener thread to capture composite signal...\n";
        WaitForSingleObject(ctx.hCompositeEvent, INFINITE);
        if (ctx.connectionClosed) break;

        cout << "  [TX THREAD] Composite signal received and decoded by Listener Thread!\n";

        // Send round acknowledgment to server
        stringstream ackSS;
        ackSS << "ROUND_ACK " << ctx.stationID << " " << ctx.lastDecodedCount << " ALL_OK";
        sendTcpLine(sock, ackSS.str());

        // For single command-line execution, exit after round 1
        if (!customMessage.empty()) {
            break;
        }

        cout << "Press ENTER for next round (or type 'exit' to quit): ";
        string choice;
        getline(cin, choice);
        if (choice == "exit" || choice == "quit") {
            sendTcpLine(sock, "EXIT");
            break;
        }
        round++;
    }

cleanup:
    cout << "\n[SHUTDOWN] Stopping background RX listener thread...\n";
    ctx.stopListener.store(true);
    closeSocket(sock);

    if (hListener != NULL) {
        WaitForSingleObject(hListener, 1000);
        CloseHandle(hListener);
    }

    CloseHandle(ctx.hConfigEvent);
    CloseHandle(ctx.hTxReadyEvent);
    CloseHandle(ctx.hCompositeEvent);
    DeleteCriticalSection(&ctx.dataCs);

    cleanupSocketSubsystem();
    DeleteCriticalSection(&g_console_cs);
    cout << "[DONE] Station terminated cleanly.\n";
    return 0;
}
