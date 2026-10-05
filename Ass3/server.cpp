/**
 * ============================================================================
 * Assignment 3: CSMA Network Environment Emulation
 * File: server.cpp
 * 
 * Description:
 *   TCP server emulating the shared physical medium (Ethernet bus).
 *   - Binds to Port 5050 on 127.0.0.1.
 *   - Emulates a shared half-duplex medium with states:
 *       activeTransmitters == 0  -->  IDLE
 *       activeTransmitters == 1  -->  BUSY
 *       activeTransmitters >= 2  -->  COLLISION
 *   - Accepts multiple concurrent station connections via non-blocking select().
 *   - When activeTransmitters >= 2, immediately broadcasts "COLLISION\n" to all
 *     currently transmitting stations so their concurrent listener threads
 *     can immediately trigger an early abort and JAM sequence.
 *   - Upon successful delivery without collision, deserializes the IEEE 802 frame
 *     and verifies 32-bit CRC (FCS) integrity.
 * 
 * Compilation (MinGW / GCC on Windows):
 *   g++ -O2 -std=c++11 server.cpp frame.cpp socketCommon.cpp -o server.exe -lws2_32
 * ============================================================================
 */

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <sstream>
#include <cstring>
#include <atomic>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "frame.h"
#include "socketCommon.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// Atomic counter of active transmissions on the shared medium
std::atomic<int> activeTransmitters{0};

// Helper to get string representation of channel state
string getChannelStateName(int activeCount) {
    if (activeCount == 0) return "IDLE";
    if (activeCount == 1) return "BUSY";
    return "COLLISION";
}

int main(int argc, char* argv[]) {
    // 1. Initialize Winsock
    if (!initializeSocketSubsystem()) {
        cerr << "[ERROR] Winsock initialization failed.\n";
        return 1;
    }

    // Determine port (default: 5050, or from command line: server.exe [port])
    int port = 5050;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 1024 || port > 65535) port = 5050;
    }

    // 2. Create TCP server listening socket
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        cerr << "[ERROR] Could not create server socket: " << WSAGetLastError() << endl;
        cleanupSocketSubsystem();
        return 1;
    }

    int optVal = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&optVal, sizeof(optVal));

    // 3. Bind to port 5050
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "[ERROR] Bind failed on port " << port << ": " << WSAGetLastError() << endl;
        closeSocket(listenSocket);
        cleanupSocketSubsystem();
        return 1;
    }

    // 4. Start listening
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "[ERROR] Listen failed: " << WSAGetLastError() << endl;
        closeSocket(listenSocket);
        cleanupSocketSubsystem();
        return 1;
    }

    cout << "========================================================\n";
    cout << "     CSMA/CD SHARED CHANNEL SERVER (Port: " << port << ")\n";
    cout << "========================================================\n";
    cout << "[SERVER] Shared medium initialized on Port " << port << ".\n";
    cout << "[SERVER] Channel state: IDLE\n";
    cout << "[SERVER] Waiting for contending station clients to connect...\n";
    cout << "--------------------------------------------------------\n\n";

    vector<SOCKET> clientSockets;
    map<SOCKET, int> socketToStationId;
    map<int, SOCKET> stationIdToSocket;
    set<int> transmittingStations;
    set<int> collidedStations;
    map<int, string> stationFrameData;

    // Simulation metrics
    long long totalSenseCount = 0;
    long long totalTxAttempts = 0;
    long long totalCollisions = 0;
    long long totalSuccesses  = 0;

    map<SOCKET, string> clientBuffers;

    // 5. Server Event Loop using non-blocking select()
    while (true) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(listenSocket, &readSet);

        for (SOCKET clientSock : clientSockets) {
            FD_SET(clientSock, &readSet);
        }

        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 15000; // 15 ms polling interval

        int activity = select(0, &readSet, NULL, NULL, &timeout);
        if (activity == SOCKET_ERROR) {
            cerr << "[ERROR] select() error: " << WSAGetLastError() << endl;
            break;
        }

        // Accept new station connections
        if (FD_ISSET(listenSocket, &readSet)) {
            sockaddr_in clientAddr;
            int clientAddrLen = sizeof(clientAddr);
            SOCKET newClient = accept(listenSocket, (sockaddr*)&clientAddr, &clientAddrLen);
            if (newClient != INVALID_SOCKET) {
                clientSockets.push_back(newClient);
                clientBuffers[newClient] = "";
                cout << "[SERVER] Station connected. Total connected stations: " 
                     << clientSockets.size() << endl;
            }
        }

        // Process incoming client commands
        for (size_t i = 0; i < clientSockets.size(); ) {
            SOCKET clientSock = clientSockets[i];

            if (FD_ISSET(clientSock, &readSet)) {
                char recvBuf[1024];
                int bytesRead = recv(clientSock, recvBuf, sizeof(recvBuf) - 1, 0);

                if (bytesRead <= 0) {
                    int stationId = socketToStationId[clientSock];
                    cout << "[SERVER] Station " << stationId << " disconnected." << endl;

                    if (transmittingStations.count(stationId)) {
                        transmittingStations.erase(stationId);
                        collidedStations.erase(stationId);
                        stationFrameData.erase(stationId);
                        stationIdToSocket.erase(stationId);
                        if (activeTransmitters > 0) activeTransmitters--;
                        cout << "[SERVER] Station " << stationId << " stopped transmission (disconnect)\n";
                        cout << "[SERVER] Channel state: " << getChannelStateName(activeTransmitters.load()) << endl;
                    }

                    closeSocket(clientSock);
                    clientBuffers.erase(clientSock);
                    socketToStationId.erase(clientSock);
                    clientSockets.erase(clientSockets.begin() + i);
                    continue;
                }

                recvBuf[bytesRead] = '\0';
                clientBuffers[clientSock] += recvBuf;

                size_t newlinePos;
                while ((newlinePos = clientBuffers[clientSock].find('\n')) != string::npos) {
                    string line = clientBuffers[clientSock].substr(0, newlinePos);
                    clientBuffers[clientSock].erase(0, newlinePos + 1);

                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (line.empty()) continue;

                    stringstream ss(line);
                    string cmd;
                    ss >> cmd;

                    // ----------------------------------------------------
                    // 1. SENSE: Check channel state
                    // ----------------------------------------------------
                    if (cmd == "SENSE") {
                        totalSenseCount++;
                        string reply;
                        if (activeTransmitters.load() == 0) {
                            reply = "IDLE\n";
                        } else {
                            reply = "BUSY\n";
                        }
                        send(clientSock, reply.c_str(), (int)reply.length(), 0);
                    }
                    // ----------------------------------------------------
                    // 2. TX_START <stationId> [bitstream]: Begin transmission
                    // ----------------------------------------------------
                    else if (cmd == "TX_START" || cmd == "START") {
                        int stId = 0;
                        string bitstream = "";
                        ss >> stId;
                        if (ss >> bitstream) {
                            stationFrameData[stId] = bitstream;
                        } else {
                            stationFrameData[stId] = "";
                        }

                        socketToStationId[clientSock] = stId;
                        stationIdToSocket[stId] = clientSock;

                        transmittingStations.insert(stId);
                        totalTxAttempts++;

                        // Increment active transmitters
                        activeTransmitters++;

                        cout << "[SERVER] Station " << stId << " started transmission\n";
                        cout << "[SERVER] Channel state: " << getChannelStateName(activeTransmitters.load()) << "\n";

                        if (activeTransmitters.load() == 1) {
                            // Channel is occupied by exactly 1 transmitter (BUSY)
                            string reply = "OK\n";
                            send(clientSock, reply.c_str(), (int)reply.length(), 0);
                        } else {
                            // Counter >= 2: COLLISION OCCURRED!
                            totalCollisions++;

                            for (int id : transmittingStations) {
                                collidedStations.insert(id);
                            }

                            cout << "[SERVER] Broadcasting collision notification to active stations\n";
                            cout << "  Colliding stations: ";
                            for (int id : transmittingStations) cout << "Station " << id << " ";
                            cout << "| Total Collisions: " << totalCollisions << "\n";

                            // Broadcast 'COLLISION' to all currently transmitting sockets
                            for (int id : transmittingStations) {
                                if (stationIdToSocket.count(id)) {
                                    SOCKET destSock = stationIdToSocket[id];
                                    string alert = "COLLISION\n";
                                    send(destSock, alert.c_str(), (int)alert.length(), 0);
                                }
                            }
                        }
                    }
                    // ----------------------------------------------------
                    // 3. DATA <stationId> <chunk>: Streaming bits
                    // ----------------------------------------------------
                    else if (cmd == "DATA") {
                        int stId = 0;
                        string chunk = "";
                        ss >> stId >> chunk;
                        stationFrameData[stId] += chunk;

                        // If collision already occurred, ensure transmitter receives alert
                        if (collidedStations.count(stId)) {
                            string alert = "COLLISION\n";
                            send(clientSock, alert.c_str(), (int)alert.length(), 0);
                        }
                    }
                    // ----------------------------------------------------
                    // 4. JAM <stationId> (CSMA/CD early collision abort)
                    // ----------------------------------------------------
                    else if (cmd == "JAM" || cmd == "ABORT") {
                        int stId = 0;
                        ss >> stId;

                        transmittingStations.erase(stId);
                        collidedStations.erase(stId);
                        stationFrameData.erase(stId);

                        if (activeTransmitters > 0) {
                            activeTransmitters--;
                        }

                        cout << "[SERVER] Station " << stId << " stopped transmission (JAM received)\n";
                        cout << "[SERVER] Channel state: " << getChannelStateName(activeTransmitters.load()) << "\n";

                        string reply = "ABORT_OK\n";
                        send(clientSock, reply.c_str(), (int)reply.length(), 0);
                    }
                    // ----------------------------------------------------
                    // 5. TX_END <stationId>: Conclude frame transmission
                    // ----------------------------------------------------
                    else if (cmd == "TX_END" || cmd == "END") {
                        int stId = 0;
                        ss >> stId;

                        bool wasCollided = (collidedStations.count(stId) > 0);
                        string bitstream = stationFrameData[stId];

                        transmittingStations.erase(stId);
                        collidedStations.erase(stId);
                        stationFrameData.erase(stId);

                        if (activeTransmitters > 0) {
                            activeTransmitters--;
                        }

                        cout << "[SERVER] Station " << stId << " stopped transmission\n";
                        cout << "[SERVER] Channel state: " << getChannelStateName(activeTransmitters.load()) << "\n";

                        if (wasCollided) {
                            string reply = "COLLISION\n";
                            send(clientSock, reply.c_str(), (int)reply.length(), 0);
                            cout << "[SERVER] Frame from Station " << stId << " completed with: [COLLISION]\n";
                        } else {
                            totalSuccesses++;
                            string reply = "SUCCESS\n";
                            send(clientSock, reply.c_str(), (int)reply.length(), 0);

                            // Deserialize and verify CRC-32 integrity
                            Frame rxFrame;
                            bool valid = Frame::deserialize(bitstream, 32, rxFrame);
                            string payloadText = "";
                            bool crcOk = false;

                            if (valid) {
                                crcOk = verifyCRC32(rxFrame.getHeaderAndPayload() + rxFrame.tailer.fcs);
                                payloadText = binaryToText(rxFrame.payload.data);
                            }

                            cout << "[SERVER] Frame from Station " << stId << " completed with: [SUCCESS]\n";
                            cout << "  Frame Seq: " << (valid ? rxFrame.header.getSequenceNumber() : 0)
                                 << " | Payload: \"" << payloadText << "\""
                                 << " | CRC-32 FCS: " << (crcOk ? "VALID (MATCH)" : "ERROR") << "\n";
                        }
                    }
                    // ----------------------------------------------------
                    // 6. STATS
                    // ----------------------------------------------------
                    else if (cmd == "STATS") {
                        stringstream statsOut;
                        statsOut << "Attempts=" << totalTxAttempts 
                                 << " Success=" << totalSuccesses 
                                 << " Collisions=" << totalCollisions 
                                 << " Senses=" << totalSenseCount << "\n";
                        string reply = statsOut.str();
                        send(clientSock, reply.c_str(), (int)reply.length(), 0);
                    }
                }
            }
            i++;
        }
    }

    for (SOCKET s : clientSockets) closeSocket(s);
    closeSocket(listenSocket);
    cleanupSocketSubsystem();
    return 0;
}
