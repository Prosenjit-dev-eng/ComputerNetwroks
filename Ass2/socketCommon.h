#ifndef SOCKETCOMMON_H
#define SOCKETCOMMON_H

#include <string>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET SocketType;
#define IS_INVALID_SOCKET(s) ((s) == INVALID_SOCKET)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
typedef int SocketType;
#define INVALID_SOCKET (-1)
#define IS_INVALID_SOCKET(s) ((s) < 0)
#endif

bool initializeSocketSubsystem();
void cleanupSocketSubsystem();
SocketType createUdpSocket();
void closeSocket(SocketType sock);
bool setSocketTimeout(SocketType sock, int timeoutMs);
bool setSocketNonBlocking(SocketType sock, bool nonBlocking);

bool sendUdp(SocketType sock, const std::string &data, const sockaddr_in &destAddr);
bool recvUdp(SocketType sock, std::string &outData, sockaddr_in &fromAddr, int maxLen = 65535);

#endif
