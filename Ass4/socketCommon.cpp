#include "socketCommon.h"
#include <vector>

using namespace std;

bool initializeSocketSubsystem()
{
#ifdef _WIN32
    static bool initialized = false;
    if (!initialized)
    {
        WSADATA wsa;
        int res = WSAStartup(MAKEWORD(2, 2), &wsa);
        if (res != 0) {
            cerr << "WSAStartup failed: " << res << endl;
            return false;
        }
        initialized = true;
    }
#endif
    return true;
}

void cleanupSocketSubsystem()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

SocketType createUdpSocket()
{
    initializeSocketSubsystem();
    SocketType sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    return sock;
}

void closeSocket(SocketType sock)
{
    if (!IS_INVALID_SOCKET(sock))
    {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
    }
}

bool setSocketTimeout(SocketType sock, int timeoutMs)
{
    if (IS_INVALID_SOCKET(sock)) return false;

#ifdef _WIN32
    DWORD timeout = timeoutMs;
    int res = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    int res = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const void*)&tv, sizeof(tv));
#endif
    return (res == 0);
}

bool setSocketNonBlocking(SocketType sock, bool nonBlocking)
{
    if (IS_INVALID_SOCKET(sock)) return false;

#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    return (ioctlsocket(sock, FIONBIO, &mode) == 0);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return false;
    flags = nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return (fcntl(sock, F_FL_SET, flags) == 0);
#endif
}

bool sendUdp(SocketType sock, const string &data, const sockaddr_in &destAddr)
{
    if (IS_INVALID_SOCKET(sock)) return false;

    int sent = sendto(sock, data.c_str(), (int)data.length(), 0,
                      (const sockaddr*)&destAddr, sizeof(destAddr));
    return (sent == (int)data.length());
}

bool recvUdp(SocketType sock, string &outData, sockaddr_in &fromAddr, int maxLen)
{
    if (IS_INVALID_SOCKET(sock)) return false;

    vector<char> buffer(maxLen);
#ifdef _WIN32
    int fromLen = sizeof(fromAddr);
#else
    socklen_t fromLen = sizeof(fromAddr);
#endif

    int received = recvfrom(sock, buffer.data(), maxLen, 0,
                            (sockaddr*)&fromAddr, &fromLen);

    if (received > 0)
    {
        outData.assign(buffer.data(), received);
        return true;
    }
    return false;
}
