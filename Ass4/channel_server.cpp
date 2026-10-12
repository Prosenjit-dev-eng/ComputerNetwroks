/**
 * ============================================================================
 * Assignment 4: CDMA Network Environment Emulation
 * File: channel_server.cpp
 * 
 * Description:
 *   TCP server emulating the shared CDMA physical medium.
 *   - Binds to Port 5050 (or user-specified port) on 127.0.0.1.
 *   - Generates the Hadamard-Walsh matrix W_N for n contending stations.
 *   - Assigns a unique orthogonal Walsh code word C_i to each station.
 *   - Synchronizes concurrent transmissions from n stations.
 *   - Linearly superimposes (adds) all station chip signals:
 *       C_composite[t] = SUM(S_i[t]) for i = 0 to n-1
 *   - Broadcasts the composite signal across the common channel to all stations.
 *   - Demonstrates multi-access without collisions via orthogonal spreading!
 * 
 * Compilation (MinGW / GCC on Windows):
 *   g++ -O2 -std=c++11 channel_server.cpp cdma.cpp frame.cpp socketCommon.cpp -o channel_server.exe -lws2_32
 * ============================================================================
 */

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "cdma.h"
#include "frame.h"
#include "socketCommon.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// Helper to send a clean line-delimited message over TCP
bool sendLine(SOCKET s, const string &msg) {
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

// Helper to read a line-delimited message from TCP socket
bool readLine(SOCKET s, string &line) {
    line = "";
    char ch;
    while (true) {
        int n = recv(s, &ch, 1, 0);
        if (n <= 0) return false;
        if (ch == '\n') break;
        if (ch != '\r') line += ch;
    }
    return true;
}

int main(int argc, char* argv[]) {
    // 1. Initialize Winsock
    if (!initializeSocketSubsystem()) {
        cerr << "[ERROR] Winsock initialization failed.\n";
        return 1;
    }

    int port = 5050;
    int numStations = 4;

    if (argc > 1) port = atoi(argv[1]);
    if (argc > 2) numStations = atoi(argv[2]);

    if (numStations < 2) numStations = 2;
    int walshOrder = nextPowerOf2(numStations);

    cout << "===================================================================\n";
    cout << "       CDMA COMMON CHANNEL SERVER (Port: " << port << ")\n";
    cout << "===================================================================\n";
    cout << "[INIT] Configured Stations: " << numStations << " stations.\n";
    cout << "[INIT] Walsh Code Order:    N = " << walshOrder << " (Smallest 2^k >= " << numStations << ")\n\n";

    // 2. Generate Walsh Matrix & Verify Orthogonality
    vector<vector<int>> walshMatrix = generateWalshMatrix(walshOrder);
    printWalshMatrix(walshMatrix, cout);
    cout << "\n";
    verifyWalshOrthogonality(walshMatrix, &cout);
    cout << "===================================================================\n\n";

    // 3. Create listening socket
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        cerr << "[ERROR] Could not create server socket: " << WSAGetLastError() << "\n";
        cleanupSocketSubsystem();
        return 1;
    }

    int optVal = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&optVal, sizeof(optVal));

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "[ERROR] Bind failed on port " << port << ": " << WSAGetLastError() << "\n";
        closeSocket(listenSocket);
        cleanupSocketSubsystem();
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "[ERROR] Listen failed: " << WSAGetLastError() << "\n";
        closeSocket(listenSocket);
        cleanupSocketSubsystem();
        return 1;
    }

    cout << "[INFO] Waiting for " << numStations << " stations to connect...\n";

    vector<SOCKET> clientSockets;
    vector<string> clientNames;

    for (int i = 0; i < numStations; i++) {
        sockaddr_in clientAddr;
        int clientLen = sizeof(clientAddr);
        SOCKET s = accept(listenSocket, (sockaddr*)&clientAddr, &clientLen);
        if (s == INVALID_SOCKET) {
            cerr << "[ERROR] Failed to accept station " << i << "\n";
            for (SOCKET cs : clientSockets) closeSocket(cs);
            closeSocket(listenSocket);
            cleanupSocketSubsystem();
            return 1;
        }

        clientSockets.push_back(s);
        stringstream nameSS;
        nameSS << "Station_" << i;
        clientNames.push_back(nameSS.str());

        char* clientIP = inet_ntoa(clientAddr.sin_addr);
        cout << "[CONNECT] " << clientNames[i] << " connected from " << clientIP 
             << ":" << ntohs(clientAddr.sin_port) 
             << " --> Assigned Walsh Code C_" << i << ": " 
             << walshVectorToString(walshMatrix[i]) << "\n";

        // Send CONFIG: CONFIG <id> <numStations> <walshOrder> <chips...>
        stringstream cfgSS;
        cfgSS << "CONFIG " << i << " " << numStations << " " << walshOrder;
        for (int chip : walshMatrix[i]) {
            cfgSS << " " << chip;
        }
        // Also send full Walsh matrix so station can despread all other stations
        for (int r = 0; r < numStations; r++) {
            for (int chip : walshMatrix[r]) {
                cfgSS << " " << chip;
            }
        }
        sendLine(s, cfgSS.str());
    }

    cout << "\n[INFO] All " << numStations << " stations connected! Channel is ACTIVE.\n";
    cout << "===================================================================\n\n";

    int roundNum = 1;
    while (true) {
        cout << "\n>>> [ROUND " << roundNum << "] Synchronizing Station Transmissions <<<\n";

        // Signal all stations to transmit
        for (int i = 0; i < numStations; i++) {
            if (!sendLine(clientSockets[i], "TX_READY")) {
                cout << "[DISCONNECT] Station " << i << " disconnected. Terminating session.\n";
                goto finish;
            }
        }

        // Receive CHIPS from all stations
        // Format: CHIPS <station_id> <num_chips> <c0> <c1> ...
        vector<vector<int>> stationChips(numStations);
        bool allReceived = true;

        for (int i = 0; i < numStations; i++) {
            string line;
            if (!readLine(clientSockets[i], line)) {
                allReceived = false;
                break;
            }

            stringstream ss(line);
            string tag;
            int sid = -1, chipCount = 0;
            ss >> tag >> sid >> chipCount;

            if (tag == "EXIT") {
                cout << "[EXIT] Station " << i << " requested session exit.\n";
                goto finish;
            }

            if (tag != "CHIPS" || sid < 0 || sid >= numStations) {
                cerr << "[ERROR] Invalid packet from station " << i << ": " << line << "\n";
                allReceived = false;
                break;
            }

            stationChips[sid].resize(chipCount);
            for (int c = 0; c < chipCount; c++) {
                ss >> stationChips[sid][c];
            }

            cout << "  Station " << sid << " Transmitted: " << chipCount << " chips (Walsh C_" << sid << ")\n";
        }

        if (!allReceived) {
            cout << "[ERROR] Did not receive all transmissions. Closing channel.\n";
            break;
        }

        // Superimpose the signals (Linear Addition on Common Medium)
        vector<int> composite = superimposeSignals(stationChips);

        cout << "\n  [COMMON CHANNEL SUPERPOSITION]:\n";
        cout << "  Composite Signal Length: " << composite.size() << " chips\n";
        cout << "  First 32 Composite Chips: [";
        size_t showChips = min((size_t)32, composite.size());
        for (size_t c = 0; c < showChips; c++) {
            if (composite[c] >= 0) cout << "+" << composite[c];
            else cout << composite[c];
            if (c + 1 < showChips) cout << ", ";
        }
        if (composite.size() > 32) cout << ", ...";
        cout << "]\n";

        // Broadcast Composite Signal to all stations
        // Format: COMPOSITE <num_chips> <c0> <c1> ...
        stringstream bcastSS;
        bcastSS << "COMPOSITE " << composite.size();
        for (int val : composite) {
            bcastSS << " " << val;
        }
        string bcastPacket = bcastSS.str();

        for (int i = 0; i < numStations; i++) {
            sendLine(clientSockets[i], bcastPacket);
        }

        cout << "  [BROADCAST] Composite signal delivered to all " << numStations << " stations.\n";

        // Wait for round acknowledgment / reports
        for (int i = 0; i < numStations; i++) {
            string ackLine;
            if (readLine(clientSockets[i], ackLine)) {
                // e.g. "ROUND_ACK <sid> <bits_decoded> <crc_status>"
                cout << "  [ACK] " << clientNames[i] << " -> " << ackLine << "\n";
            }
        }

        cout << "-------------------------------------------------------------------\n";
        roundNum++;
    }

finish:
    cout << "\n[SHUTDOWN] Closing server and cleaning up sockets...\n";
    for (SOCKET cs : clientSockets) closeSocket(cs);
    closeSocket(listenSocket);
    cleanupSocketSubsystem();
    cout << "[DONE] Server exited successfully.\n";
    return 0;
}
