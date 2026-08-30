#ifndef SOCKETCOMMON_H
#define SOCKETCOMMON_H

#include <string>

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET SocketType;
#else
typedef int SocketType;
#endif

bool sendAll(SocketType sock, const char* data, int length);
bool recvAll(SocketType sock, char* data, int length);
bool sendMessage(SocketType sock, const std::string& message);
bool receiveMessage(SocketType sock, std::string& message);
void closeSocket(SocketType sock);

#endif
