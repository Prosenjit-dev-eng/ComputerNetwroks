/**
 * ============================================================================
 * Assignment 3: CSMA Multi-Station Network Environment Simulation
 * File: simulation.cpp
 * 
 * Description:
 *   All-in-one multithreaded CSMA simulation program.
 *   - Runs the Shared Channel (Medium) server in a background thread.
 *   - Spawns N contending station threads using a for loop.
 *   - No multiple terminals needed: runs completely in a single console!
 *   - Tracks and displays per-station & aggregate performance metrics:
 *       * Frames Attempted
 *       * Frames Delivered
 *       * Frame Drops Count (due to max backoff attempts exceeded)
 *       * Collision Drops / Total Collisions Count
 *       * Average Transmission Delay
 *       * Throughput & Channel Efficiency
 * 
 * Supported CSMA Schemes:
 *   1: 1-Persistent CSMA
 *   2: Non-Persistent CSMA
 *   3: p-Persistent CSMA
 *   4: CSMA/CD (Carrier Sense Multiple Access with Collision Detection)
 * 
 * Compilation (MinGW / GCC on Windows):
 *   g++ -O2 -std=c++11 simulation.cpp frame.cpp socketCommon.cpp -o simulation.exe -lws2_32
 * 
 * Execution:
 *   simulation.exe [num_stations] [strategy: 1-4] [num_frames] [prob_p] [port]
 *   Example: simulation.exe 5 4 3
 * ============================================================================
 */

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <atomic>
#include <algorithm>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "frame.h"
#include "socketCommon.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;
using namespace chrono;

// ============================================================================
// TIMING & PROTOCOL CONSTANTS
// ============================================================================
const int SLOT_TIME_MS          = 40;   // Base slot time for slotted medium & backoff unit
const int CHUNK_DELAY_MS        = 15;   // Delay per chunk to emulate physical bit propagation
const int CHUNK_SIZE_BITS       = 16;   // Number of bits transmitted per chunk
const int MAX_BACKOFF_ATTEMPTS  = 15;   // Maximum retry attempts before dropping frame

// Thread-safe console printing
CRITICAL_SECTION g_console_cs;

void printLog(const string &msg) {
    EnterCriticalSection(&g_console_cs);
    cout << msg << flush;
    LeaveCriticalSection(&g_console_cs);
}

// ============================================================================
// SERVER DATA STRUCTURES & THREAD
// ============================================================================
struct ServerConfig {
    int port;
    std::atomic<bool> stopFlag{false};
    std::atomic<bool> readyFlag{false};

    // Global server metrics
    std::atomic<long long> totalSenses{0};
    std::atomic<long long> totalTxAttempts{0};
    std::atomic<long long> totalCollisions{0};
    std::atomic<long long> totalSuccesses{0};
};

string getChannelStateName(int activeCount) {
    if (activeCount == 0) return "IDLE";
    if (activeCount == 1) return "BUSY";
    return "COLLISION";
}

DWORD WINAPI serverThreadFunc(LPVOID lpParam) {
    ServerConfig* cfg = (ServerConfig*)lpParam;
    int port = cfg->port;

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        printLog("[SERVER ERROR] Could not create server socket.\n");
        return 1;
    }

    int optVal = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&optVal, sizeof(optVal));

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        stringstream ss;
        ss << "[SERVER ERROR] Bind failed on port " << port << " (Code: " << WSAGetLastError() << ")\n";
        printLog(ss.str());
        closeSocket(listenSocket);
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        printLog("[SERVER ERROR] Listen failed.\n");
        closeSocket(listenSocket);
        return 1;
    }

    cfg->readyFlag.store(true);

    vector<SOCKET> clientSockets;
    map<SOCKET, int> socketToStationId;
    map<int, SOCKET> stationIdToSocket;
    set<int> transmittingStations;
    set<int> collidedStations;
    map<int, string> stationFrameData;
    map<SOCKET, string> clientBuffers;
    std::atomic<int> activeTransmitters{0};

    while (!cfg->stopFlag.load()) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(listenSocket, &readSet);

        for (SOCKET clientSock : clientSockets) {
            FD_SET(clientSock, &readSet);
        }

        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000; // 10 ms polling

        int activity = select(0, &readSet, NULL, NULL, &timeout);
        if (activity == SOCKET_ERROR) {
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
            }
        }

        // Process station messages
        for (size_t i = 0; i < clientSockets.size(); ) {
            SOCKET clientSock = clientSockets[i];

            if (FD_ISSET(clientSock, &readSet)) {
                char recvBuf[1024];
                int bytesRead = recv(clientSock, recvBuf, sizeof(recvBuf) - 1, 0);

                if (bytesRead <= 0) {
                    int stationId = socketToStationId[clientSock];
                    if (transmittingStations.count(stationId)) {
                        transmittingStations.erase(stationId);
                        collidedStations.erase(stationId);
                        stationFrameData.erase(stationId);
                        stationIdToSocket.erase(stationId);
                        if (activeTransmitters > 0) activeTransmitters--;
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

                    // 1. SENSE
                    if (cmd == "SENSE") {
                        cfg->totalSenses++;
                        string reply = (activeTransmitters.load() == 0) ? "IDLE\n" : "BUSY\n";
                        send(clientSock, reply.c_str(), (int)reply.length(), 0);
                    }
                    // 2. TX_START <id> [bitstream]
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
                        cfg->totalTxAttempts++;
                        activeTransmitters++;

                        if (activeTransmitters.load() == 1) {
                            string reply = "OK\n";
                            send(clientSock, reply.c_str(), (int)reply.length(), 0);
                        } else {
                            // COLLISION OCCURRED!
                            cfg->totalCollisions++;
                            for (int id : transmittingStations) {
                                collidedStations.insert(id);
                            }

                            // Broadcast COLLISION to all currently transmitting stations
                            for (int id : transmittingStations) {
                                if (stationIdToSocket.count(id)) {
                                    SOCKET destSock = stationIdToSocket[id];
                                    string alert = "COLLISION\n";
                                    send(destSock, alert.c_str(), (int)alert.length(), 0);
                                }
                            }
                        }
                    }
                    // 3. DATA <id> <chunk>
                    else if (cmd == "DATA") {
                        int stId = 0;
                        string chunk = "";
                        ss >> stId >> chunk;
                        stationFrameData[stId] += chunk;

                        if (collidedStations.count(stId)) {
                            string alert = "COLLISION\n";
                            send(clientSock, alert.c_str(), (int)alert.length(), 0);
                        }
                    }
                    // 4. JAM <id>
                    else if (cmd == "JAM" || cmd == "ABORT") {
                        int stId = 0;
                        ss >> stId;
                        transmittingStations.erase(stId);
                        collidedStations.erase(stId);
                        stationFrameData.erase(stId);
                        if (activeTransmitters > 0) activeTransmitters--;

                        string reply = "ABORT_OK\n";
                        send(clientSock, reply.c_str(), (int)reply.length(), 0);
                    }
                    // 5. TX_END <id>
                    else if (cmd == "TX_END" || cmd == "END") {
                        int stId = 0;
                        ss >> stId;

                        bool wasCollided = (collidedStations.count(stId) > 0);
                        string bitstream = stationFrameData[stId];

                        transmittingStations.erase(stId);
                        collidedStations.erase(stId);
                        stationFrameData.erase(stId);
                        if (activeTransmitters > 0) activeTransmitters--;

                        if (wasCollided) {
                            string reply = "COLLISION\n";
                            send(clientSock, reply.c_str(), (int)reply.length(), 0);
                        } else {
                            cfg->totalSuccesses++;
                            string reply = "SUCCESS\n";
                            send(clientSock, reply.c_str(), (int)reply.length(), 0);
                        }
                    }
                }
            }
            i++;
        }
    }

    for (SOCKET s : clientSockets) closeSocket(s);
    closeSocket(listenSocket);
    return 0;
}

// ============================================================================
// PER-STATION STATE & METRICS
// ============================================================================
struct StationState {
    int stationId;
    int port;
    int strategy;
    double probP;
    int numFrames;

    SOCKET sock;
    HANDLE hSenseEvent;
    HANDLE hEndEvent;
    CRITICAL_SECTION replyCs;
    string endReplyStr;

    std::atomic<bool> collisionDetected{false};
    std::atomic<bool> stopListener{false};
    std::atomic<int>  channelState{0}; // 0: IDLE, 1: BUSY

    // Metrics for this station
    int framesAttempted{0};
    int framesDelivered{0};
    int framesDropped{0};       // Dropped due to 15 backoffs exhausted
    int collisionDrops{0};      // Collisions that caused drop or retry
    int totalCollisions{0};
    double totalDelayMs{0.0};
    long long totalPayloadBits{0};
};

Frame createIEEE802Frame(int stationId, int seqNum, const string &rawMessage) {
    stringstream ssSrc;
    ssSrc << "00:11:22:33:44:" << setfill('0') << setw(2) << hex << uppercase << (stationId & 0xFF);
    string srcMacBin  = macToBinary(ssSrc.str());
    string destMacBin = macToBinary("FF:FF:FF:FF:FF:FF");

    string payloadText = rawMessage;
    if (payloadText.length() < FrameConstants::MIN_PAYLOAD_BYTES) {
        payloadText.append(FrameConstants::MIN_PAYLOAD_BYTES - payloadText.length(), '-');
    }

    string payloadBin = textToBinary(payloadText);
    string lengthBin  = uint16ToBinary((unsigned short)payloadText.length());
    string seqBin     = byteToBinary((unsigned char)seqNum);

    Header header(srcMacBin, destMacBin, lengthBin, seqBin);
    Payload payload(payloadBin);

    string hAndP = header.srcMAC + header.destMAC + header.lengthField + header.seqField + payload.data;
    string fcsBin = computeCRC32(hAndP);

    Tailer tailer(fcsBin);
    return Frame(header, payload, tailer);
}

// Background Listener Thread for each station
DWORD WINAPI stationListenerThread(LPVOID lpParam) {
    StationState* st = (StationState*)lpParam;
    char buffer[1024];
    string stream = "";

    while (!st->stopListener.load()) {
        int bytesRead = recv(st->sock, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) break;

        buffer[bytesRead] = '\0';
        stream += buffer;

        size_t newlinePos;
        while ((newlinePos = stream.find('\n')) != string::npos) {
            string line = stream.substr(0, newlinePos);
            stream.erase(0, newlinePos + 1);

            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            if (line.find("COLLISION") != string::npos) {
                st->collisionDetected.store(true);
                st->channelState.store(1);

                EnterCriticalSection(&st->replyCs);
                st->endReplyStr = "COLLISION";
                LeaveCriticalSection(&st->replyCs);

                SetEvent(st->hEndEvent);
                SetEvent(st->hSenseEvent);
            }
            else if (line == "IDLE") {
                st->channelState.store(0);
                SetEvent(st->hSenseEvent);
            }
            else if (line == "BUSY") {
                st->channelState.store(1);
                SetEvent(st->hSenseEvent);
            }
            else if (line == "SUCCESS") {
                EnterCriticalSection(&st->replyCs);
                st->endReplyStr = "SUCCESS";
                LeaveCriticalSection(&st->replyCs);
                SetEvent(st->hEndEvent);
            }
        }
    }
    return 0;
}

bool sendTcpLine(SOCKET sock, const string &text) {
    string msg = text + "\n";
    int sent = send(sock, msg.c_str(), (int)msg.length(), 0);
    return (sent == (int)msg.length());
}

string senseChannel(StationState* st) {
    ResetEvent(st->hSenseEvent);
    if (!sendTcpLine(st->sock, "SENSE")) {
        return "BUSY";
    }
    WaitForSingleObject(st->hSenseEvent, 200);
    return (st->channelState.load() == 0) ? "IDLE" : "BUSY";
}

int applyExponentialBackoff(StationState* st, int attempt) {
    int k = min(attempt, 10);
    int maxSlots = (1 << k);
    int randomSlots = rand() % maxSlots;
    int backoffMs = randomSlots * SLOT_TIME_MS;

    stringstream ss;
    ss << "   [Station " << st->stationId << "] Collision #" << attempt 
       << " -> Window W=[0.." << (maxSlots - 1) << "]. Backing off " 
       << randomSlots << " slots (" << backoffMs << " ms)\n";
    printLog(ss.str());

    Sleep(backoffMs);
    return backoffMs;
}

// ----------------------------------------------------------------------------
// CSMA/CD Transmission
// ----------------------------------------------------------------------------
bool transmitCsmaCd(StationState* st, int frameSeq, const Frame &frame, int &outCollisions, double &outDelayMs) {
    auto queueTime = high_resolution_clock::now();
    string bitstream = frame.serialize();
    int totalBits = (int)bitstream.length();

    for (int attempt = 1; attempt <= MAX_BACKOFF_ATTEMPTS; attempt++) {
        // Sense channel until IDLE
        while (senseChannel(st) != "IDLE") {
            Sleep(SLOT_TIME_MS);
        }

        st->collisionDetected.store(false);
        sendTcpLine(st->sock, "TX_START " + to_string(st->stationId));

        bool midFrameCollision = false;
        int sentBits = 0;

        for (int i = 0; i < totalBits; i += CHUNK_SIZE_BITS) {
            Sleep(CHUNK_DELAY_MS);

            int len = min(CHUNK_SIZE_BITS, totalBits - i);
            string chunk = bitstream.substr(i, len);
            sendTcpLine(st->sock, "DATA " + to_string(st->stationId) + " " + chunk);
            sentBits += len;

            if (st->collisionDetected.load()) {
                midFrameCollision = true;
                outCollisions++;

                stringstream ss;
                ss << " [Station " << st->stationId << "] *** COLLISION DETECTED at bit " 
                   << sentBits << "/" << totalBits << "! Sending JAM... ***\n";
                printLog(ss.str());

                sendTcpLine(st->sock, "JAM " + to_string(st->stationId));
                break;
            }
        }

        if (midFrameCollision) {
            applyExponentialBackoff(st, attempt);
            continue;
        }

        ResetEvent(st->hEndEvent);
        sendTcpLine(st->sock, "TX_END " + to_string(st->stationId));
        WaitForSingleObject(st->hEndEvent, 400);

        string reply;
        EnterCriticalSection(&st->replyCs);
        reply = st->endReplyStr;
        LeaveCriticalSection(&st->replyCs);

        if (st->collisionDetected.load() || reply == "COLLISION") {
            outCollisions++;
            stringstream ss;
            ss << " [Station " << st->stationId << "] Collision at frame conclusion. Backing off...\n";
            printLog(ss.str());
            applyExponentialBackoff(st, attempt);
            continue;
        }

        // SUCCESS!
        auto deliveryTime = high_resolution_clock::now();
        outDelayMs = duration_cast<milliseconds>(deliveryTime - queueTime).count();
        stringstream ss;
        ss << " [Station " << st->stationId << "] >>> SUCCESS! Frame #" << frameSeq 
           << " delivered! Delay: " << outDelayMs << " ms (Collisions: " << (attempt - 1) << ") <<<\n";
        printLog(ss.str());
        return true;
    }

    stringstream ss;
    ss << " [Station " << st->stationId << "] [DROP] Frame #" << frameSeq 
       << " dropped! (Max 15 retries exhausted)\n";
    printLog(ss.str());
    return false;
}

// ----------------------------------------------------------------------------
// 1-Persistent CSMA
// ----------------------------------------------------------------------------
bool transmit1Persistent(StationState* st, int frameSeq, const Frame &frame, int &outCollisions, double &outDelayMs) {
    auto queueTime = high_resolution_clock::now();
    string bitstream = frame.serialize();
    int totalBits = (int)bitstream.length();

    for (int attempt = 1; attempt <= MAX_BACKOFF_ATTEMPTS; attempt++) {
        while (senseChannel(st) != "IDLE") {
            Sleep(SLOT_TIME_MS);
        }

        st->collisionDetected.store(false);
        ResetEvent(st->hEndEvent);

        sendTcpLine(st->sock, "TX_START " + to_string(st->stationId) + " " + bitstream);
        Sleep(totalBits * 2);
        sendTcpLine(st->sock, "TX_END " + to_string(st->stationId));

        WaitForSingleObject(st->hEndEvent, 400);

        string reply;
        EnterCriticalSection(&st->replyCs);
        reply = st->endReplyStr;
        LeaveCriticalSection(&st->replyCs);

        if (st->collisionDetected.load() || reply == "COLLISION") {
            outCollisions++;
            stringstream ss;
            ss << " [Station " << st->stationId << "] Collision for Frame #" << frameSeq << "! Backing off...\n";
            printLog(ss.str());
            applyExponentialBackoff(st, attempt);
        } else {
            auto deliveryTime = high_resolution_clock::now();
            outDelayMs = duration_cast<milliseconds>(deliveryTime - queueTime).count();
            stringstream ss;
            ss << " [Station " << st->stationId << "] >>> SUCCESS: Frame #" << frameSeq 
               << " delivered! Delay: " << outDelayMs << " ms <<<\n";
            printLog(ss.str());
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Non-Persistent CSMA
// ----------------------------------------------------------------------------
bool transmitNonPersistent(StationState* st, int frameSeq, const Frame &frame, int &outCollisions, double &outDelayMs) {
    auto queueTime = high_resolution_clock::now();
    string bitstream = frame.serialize();
    int totalBits = (int)bitstream.length();

    for (int attempt = 1; attempt <= MAX_BACKOFF_ATTEMPTS; attempt++) {
        string state = senseChannel(st);
        if (state != "IDLE") {
            stringstream ss;
            ss << " [Station " << st->stationId << "] Channel BUSY. Non-persistent deferral...\n";
            printLog(ss.str());
            applyExponentialBackoff(st, attempt);
            continue;
        }

        st->collisionDetected.store(false);
        ResetEvent(st->hEndEvent);

        sendTcpLine(st->sock, "TX_START " + to_string(st->stationId) + " " + bitstream);
        Sleep(totalBits * 2);
        sendTcpLine(st->sock, "TX_END " + to_string(st->stationId));

        WaitForSingleObject(st->hEndEvent, 400);

        string reply;
        EnterCriticalSection(&st->replyCs);
        reply = st->endReplyStr;
        LeaveCriticalSection(&st->replyCs);

        if (st->collisionDetected.load() || reply == "COLLISION") {
            outCollisions++;
            applyExponentialBackoff(st, attempt);
        } else {
            auto deliveryTime = high_resolution_clock::now();
            outDelayMs = duration_cast<milliseconds>(deliveryTime - queueTime).count();
            stringstream ss;
            ss << " [Station " << st->stationId << "] >>> SUCCESS: Frame #" << frameSeq 
               << " delivered! Delay: " << outDelayMs << " ms <<<\n";
            printLog(ss.str());
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// p-Persistent CSMA
// ----------------------------------------------------------------------------
bool transmitPPersistent(StationState* st, int frameSeq, double p, const Frame &frame, int &outCollisions, double &outDelayMs) {
    auto queueTime = high_resolution_clock::now();
    string bitstream = frame.serialize();
    int totalBits = (int)bitstream.length();

    for (int attempt = 1; attempt <= MAX_BACKOFF_ATTEMPTS; attempt++) {
        while (senseChannel(st) != "IDLE") {
            Sleep(SLOT_TIME_MS);
        }

        bool readyToTx = false;
        while (!readyToTx) {
            double r = (double)rand() / (double)RAND_MAX;
            if (r < p) {
                readyToTx = true;
            } else {
                Sleep(SLOT_TIME_MS);
                if (senseChannel(st) != "IDLE") {
                    break;
                }
            }
        }

        if (!readyToTx) continue;

        st->collisionDetected.store(false);
        ResetEvent(st->hEndEvent);

        sendTcpLine(st->sock, "TX_START " + to_string(st->stationId) + " " + bitstream);
        Sleep(totalBits * 2);
        sendTcpLine(st->sock, "TX_END " + to_string(st->stationId));

        WaitForSingleObject(st->hEndEvent, 400);

        string reply;
        EnterCriticalSection(&st->replyCs);
        reply = st->endReplyStr;
        LeaveCriticalSection(&st->replyCs);

        if (st->collisionDetected.load() || reply == "COLLISION") {
            outCollisions++;
            applyExponentialBackoff(st, attempt);
        } else {
            auto deliveryTime = high_resolution_clock::now();
            outDelayMs = duration_cast<milliseconds>(deliveryTime - queueTime).count();
            stringstream ss;
            ss << " [Station " << st->stationId << "] >>> SUCCESS: Frame #" << frameSeq 
               << " delivered! Delay: " << outDelayMs << " ms <<<\n";
            printLog(ss.str());
            return true;
        }
    }
    return false;
}

// ============================================================================
// STATION WORKER THREAD FUNCTION
// ============================================================================
DWORD WINAPI stationThreadFunc(LPVOID lpParam) {
    StationState* st = (StationState*)lpParam;

    // Seed random per station
    srand((unsigned int)(time(NULL) + st->stationId * 2654435761u));

    InitializeCriticalSection(&st->replyCs);
    st->hSenseEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    st->hEndEvent   = CreateEvent(NULL, FALSE, FALSE, NULL);

    // Create client TCP socket
    st->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (st->sock == INVALID_SOCKET) {
        stringstream ss;
        ss << "[ERROR] Station " << st->stationId << " socket() failed.\n";
        printLog(ss.str());
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(st->port);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(st->sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        stringstream ss;
        ss << "[ERROR] Station " << st->stationId << " failed to connect to 127.0.0.1:" << st->port << "\n";
        printLog(ss.str());
        closeSocket(st->sock);
        return 1;
    }

    // Launch background listener for this station
    HANDLE hListener = CreateThread(NULL, 0, stationListenerThread, (LPVOID)st, 0, NULL);

    st->framesAttempted = st->numFrames;

    // Frame transmission loop
    for (int f = 1; f <= st->numFrames; f++) {
        string payloadText = "Payload from Station #" + to_string(st->stationId) + " Frame #" + to_string(f);
        Frame frame = createIEEE802Frame(st->stationId, f, payloadText);

        int collisions = 0;
        double delayMs = 0.0;
        bool success = false;

        switch (st->strategy) {
            case 1:
                success = transmit1Persistent(st, f, frame, collisions, delayMs);
                break;
            case 2:
                success = transmitNonPersistent(st, f, frame, collisions, delayMs);
                break;
            case 3:
                success = transmitPPersistent(st, f, st->probP, frame, collisions, delayMs);
                break;
            case 4:
            default:
                success = transmitCsmaCd(st, f, frame, collisions, delayMs);
                break;
        }

        st->totalCollisions += collisions;

        if (success) {
            st->framesDelivered++;
            st->totalDelayMs += delayMs;
            st->totalPayloadBits += (payloadText.length() * 8);
        } else {
            st->framesDropped++;
            st->collisionDrops++;
        }

        Sleep(100);
    }

    // Cleanup station socket
    st->stopListener.store(true);
    shutdown(st->sock, SD_BOTH);
    closeSocket(st->sock);

    if (hListener != NULL) {
        WaitForSingleObject(hListener, 500);
        CloseHandle(hListener);
    }

    DeleteCriticalSection(&st->replyCs);
    CloseHandle(st->hSenseEvent);
    CloseHandle(st->hEndEvent);

    return 0;
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
int main(int argc, char* argv[]) {
    InitializeCriticalSection(&g_console_cs);

    // Initialize Winsock
    if (!initializeSocketSubsystem()) {
        cerr << "[ERROR] WSAStartup failed: " << WSAGetLastError() << "\n";
        return 1;
    }

    // Parameters with defaults
    int numStations     = 5;    // Default: 5 contending stations
    int strategyChoice  = 4;    // Default: CSMA/CD
    int numFrames       = 3;    // Default: 3 frames per station
    double probP        = 0.5;  // Default: p=0.5 for p-persistent
    int port            = 5055; // Port 5055 to avoid clashes

    // CLI Argument Parsing: simulation.exe [N] [strategy] [numFrames] [p] [port]
    if (argc > 1) {
        numStations = atoi(argv[1]);
        if (numStations <= 0) numStations = 5;
    }
    if (argc > 2) {
        strategyChoice = atoi(argv[2]);
        if (strategyChoice < 1 || strategyChoice > 4) strategyChoice = 4;
    }
    if (argc > 3) {
        numFrames = atoi(argv[3]);
        if (numFrames <= 0) numFrames = 3;
    }
    if (argc > 4 && strategyChoice == 3) {
        probP = atof(argv[4]);
        if (probP <= 0.0) probP = 0.1;
        if (probP > 1.0) probP = 1.0;
    }
    if (argc > 5) {
        port = atoi(argv[5]);
        if (port <= 1024 || port > 65535) port = 5055;
    }

    string modeName = (strategyChoice == 1) ? "1-Persistent CSMA" :
                      (strategyChoice == 2) ? "Non-Persistent CSMA" :
                      (strategyChoice == 3) ? ("p-Persistent CSMA (p=" + to_string(probP) + ")") :
                                              "CSMA/CD (Listen-While-Talk + Backoff)";

    cout << "===================================================================\n";
    cout << "      CSMA MULTI-STATION ALL-IN-ONE THREADED SIMULATION            \n";
    cout << "===================================================================\n";
    cout << " Number of Contending Stations (Threads) : " << numStations << "\n";
    cout << " MAC Protocol Strategy                   : " << modeName << "\n";
    cout << " Frames per Station                      : " << numFrames << "\n";
    cout << " Total Contending Frames                 : " << (numStations * numFrames) << "\n";
    cout << " Emulated Port                           : " << port << "\n";
    cout << " Single Terminal Operation               : YES (No extra terminals needed)\n";
    cout << "===================================================================\n\n";

    // 1. Launch Server in background thread
    ServerConfig serverCfg;
    serverCfg.port = port;

    HANDLE hServerThread = CreateThread(NULL, 0, serverThreadFunc, (LPVOID)&serverCfg, 0, NULL);
    if (hServerThread == NULL) {
        cerr << "[ERROR] Failed to start Server thread!\n";
        cleanupSocketSubsystem();
        return 1;
    }

    // Wait until server is bound and ready
    while (!serverCfg.readyFlag.load()) {
        Sleep(20);
    }
    cout << "[SYSTEM] Shared Medium Server started on port " << port << ".\n";
    cout << "[SYSTEM] Spawning " << numStations << " station threads concurrently...\n\n";

    auto simStartTime = high_resolution_clock::now();

    // 2. Allocate and initialize Station State objects
    vector<StationState> stations(numStations);
    vector<HANDLE> stationThreads(numStations);

    // 3. FOR LOOP: Spawn N station threads
    for (int i = 0; i < numStations; i++) {
        stations[i].stationId = i + 1;
        stations[i].port = port;
        stations[i].strategy = strategyChoice;
        stations[i].probP = probP;
        stations[i].numFrames = numFrames;

        stationThreads[i] = CreateThread(
            NULL,
            0,
            stationThreadFunc,
            (LPVOID)&stations[i],
            0,
            NULL
        );

        if (stationThreads[i] == NULL) {
            cerr << "[ERROR] Failed to create thread for Station " << (i + 1) << "\n";
        }
    }

    // 4. FOR LOOP: Wait for all N station threads to complete
    WaitForMultipleObjects(numStations, stationThreads.data(), TRUE, INFINITE);

    for (int i = 0; i < numStations; i++) {
        CloseHandle(stationThreads[i]);
    }

    auto simEndTime = high_resolution_clock::now();
    double totalElapsedSec = duration_cast<milliseconds>(simEndTime - simStartTime).count() / 1000.0;

    // Signal server to shutdown and join
    serverCfg.stopFlag.store(true);
    WaitForSingleObject(hServerThread, 1000);
    CloseHandle(hServerThread);

    // 5. Aggregate Performance Metrics Calculation
    int aggAttempted = 0;
    int aggDelivered = 0;
    int aggDropped   = 0;
    int aggCollisionDrops = 0;
    int aggCollisions = 0;
    double aggDelaySum = 0.0;
    long long aggPayloadBits = 0;

    cout << "\n===================================================================\n";
    cout << "                    PER-STATION RESULTS SUMMARY                    \n";
    cout << "===================================================================\n";
    cout << left << setw(10) << "Station" 
         << setw(12) << "Attempted" 
         << setw(12) << "Delivered" 
         << setw(14) << "Frame Drops" 
         << setw(16) << "Collision Drops"
         << setw(14) << "Collisions" 
         << setw(14) << "Avg Delay(ms)" << "\n";
    cout << string(82, '-') << "\n";

    for (int i = 0; i < numStations; i++) {
        const auto &st = stations[i];
        aggAttempted      += st.framesAttempted;
        aggDelivered      += st.framesDelivered;
        aggDropped        += st.framesDropped;
        aggCollisionDrops += st.collisionDrops;
        aggCollisions     += st.totalCollisions;
        aggDelaySum       += st.totalDelayMs;
        aggPayloadBits    += st.totalPayloadBits;

        double stAvgDelay = (st.framesDelivered > 0) ? (st.totalDelayMs / st.framesDelivered) : 0.0;

        cout << left << setw(10) << ("#" + to_string(st.stationId))
             << setw(12) << st.framesAttempted
             << setw(12) << st.framesDelivered
             << setw(14) << st.framesDropped
             << setw(16) << st.collisionDrops
             << setw(14) << st.totalCollisions
             << fixed << setprecision(2) << setw(14) << stAvgDelay << "\n";
    }

    double avgDelayAll = (aggDelivered > 0) ? (aggDelaySum / aggDelivered) : 0.0;
    double throughputBps = (totalElapsedSec > 0) ? (aggPayloadBits / totalElapsedSec) : 0.0;
    double deliveryRate = (aggAttempted > 0) ? (aggDelivered * 100.0 / aggAttempted) : 0.0;
    double dropRate = (aggAttempted > 0) ? (aggDropped * 100.0 / aggAttempted) : 0.0;

    cout << "===================================================================\n";
    cout << "                    AGGREGATE SIMULATION METRICS                   \n";
    cout << "===================================================================\n";
    cout << " Strategy Evaluated        : " << modeName << "\n";
    cout << " Number of Stations        : " << numStations << "\n";
    cout << " Total Frames Attempted    : " << aggAttempted << "\n";
    cout << " Total Frames Delivered    : " << aggDelivered << "\n";
    cout << " ------------------------------------------------------------------\n";
    cout << " >> TOTAL FRAME DROPS      : " << aggDropped << " (" << fixed << setprecision(1) << dropRate << "%)\n";
    cout << " >> TOTAL COLLISION DROPS  : " << aggCollisionDrops << "\n";
    cout << " >> TOTAL COLLISIONS LOGGED: " << aggCollisions << "\n";
    cout << " ------------------------------------------------------------------\n";
    cout << " Channel Delivery Rate     : " << fixed << setprecision(2) << deliveryRate << " %\n";
    cout << " Average Delay per Frame   : " << fixed << setprecision(2) << avgDelayAll << " ms\n";
    cout << " Total Simulation Time     : " << fixed << setprecision(2) << totalElapsedSec << " s\n";
    cout << " Channel Throughput        : " << fixed << setprecision(2) << throughputBps << " bps\n";
    cout << "===================================================================\n\n";

    DeleteCriticalSection(&g_console_cs);
    cleanupSocketSubsystem();

    return 0;
}
