#include "socketCommon.h"
#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

bool sendAll(SocketType sock, const char* data, int length)
{
    int total = 0;

    while (total < length)
    {
        int n = send(sock, data + total, length - total, 0);

        if (n <= 0)
            return false;

        total += n;
    }

    return true;
}

bool recvAll(SocketType sock, char* data, int length)
{
    int total = 0;

    while (total < length)
    {
        int n = recv(sock, data + total, length - total, 0);

        if (n <= 0)
            return false;

        total += n;
    }

    return true;
}

bool sendMessage(SocketType sock, const std::string& message)
{
    uint32_t length = htonl((uint32_t)message.size());

    if (!sendAll(sock, (char*)&length, 4))
        return false;

    return sendAll(sock, message.c_str(), (int)message.size());
}

bool receiveMessage(SocketType sock, std::string& message)
{
    uint32_t length;

    if (!recvAll(sock, (char*)&length, 4))
        return false;

    length = ntohl(length);
    message.resize(length);

    return recvAll(sock, &message[0], (int)length);
}

void closeSocket(SocketType sock)
{
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}
