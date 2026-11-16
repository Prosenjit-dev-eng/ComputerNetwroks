#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <cerrno>
#include <cstring>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <basetsd.h>
  #pragma comment(lib, "ws2_32.lib")
  using socket_t = SOCKET;
  #define IS_INVALID_SOCKET(s) ((s) == INVALID_SOCKET)
  #define CLOSE_SOCKET(s) ::closesocket(s)
  #ifndef MSG_NOSIGNAL
    #define MSG_NOSIGNAL 0
  #endif
  using ssize_t = SSIZE_T;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <netdb.h>
  using socket_t = int;
  #define INVALID_SOCKET (-1)
  #define IS_INVALID_SOCKET(s) ((s) < 0)
  #define CLOSE_SOCKET(s) ::close(s)
  #ifndef MSG_NOSIGNAL
    #define MSG_NOSIGNAL 0
  #endif
#endif

class SocketException : public std::runtime_error {
public:
    explicit SocketException(const std::string& msg) : std::runtime_error(msg) {}
};

class Socket {
public:
    static void initNetwork() {
#ifdef _WIN32
        static struct WSAInitializer {
            WSAInitializer() {
                WSADATA wsa;
                ::WSAStartup(MAKEWORD(2, 2), &wsa);
            }
            ~WSAInitializer() {
                ::WSACleanup();
            }
        } init;
#endif
    }

    explicit Socket(socket_t fd) : fd_(fd) { initNetwork(); }
    // Domain = IPV4/IPV6(AF_INET), type = SOCK_STREAM(TCP), Protocol =  
    Socket(int domain, int type, int protocol) {
        initNetwork();
        fd_ = ::socket(domain, type, protocol);
        if (IS_INVALID_SOCKET(fd_))
            throw SocketException("socket() failed");
    }

    Socket(Socket&& other) noexcept : fd_(other.fd_) { other.fd_ = INVALID_SOCKET; }
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            closefd();
            fd_ = other.fd_;
            other.fd_ = INVALID_SOCKET;
        }
        return *this;
    }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    ~Socket() { closefd(); }
    // Socket->Creates a TCP socket
    static Socket createTCP() {
        return Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    }
    // Bind = bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    // Binds server socket to an IP and port
    void bind(const std::string& addr, uint16_t port) {
        struct sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port   = htons(port);
        if (addr.empty() || addr == "0.0.0.0") {
            sa.sin_addr.s_addr = INADDR_ANY;
        } else {
            if (::inet_pton(AF_INET, addr.c_str(), &sa.sin_addr) != 1)
                throw SocketException("Invalid bind address: " + addr);
        }
        if (::bind(fd_, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) < 0)
            throw SocketException("bind() failed");
    }
// Listen = listen(serverSocket, 5);
// Starts listening for connections
    void listen(int backlog = 5) {
        if (::listen(fd_, backlog) < 0)
            throw SocketException("listen() failed");
    }
// Accept = int clientSocket = accept(serverSocket, nullptr, nullptr);
// Accepts incoming clients
    Socket accept() {
        struct sockaddr_in ca{};
        socklen_t len = sizeof(ca);
        socket_t cfd;
        do {
            cfd = ::accept(fd_, reinterpret_cast<sockaddr*>(&ca), &len);
        } while (IS_INVALID_SOCKET(cfd) && errno == EINTR);
        if (IS_INVALID_SOCKET(cfd))
            throw SocketException("accept() failed");
        return Socket(cfd);
    }

    // For client
    // Connect = int clientSocket = accept(serverSocket, nullptr, nullptr);
    // Connects the client to the server
    void connect(const std::string& host, uint16_t port) {
        struct addrinfo hints{}, *res = nullptr;
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        std::string portStr = std::to_string(port);
        int rc = ::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res);
        if (rc != 0)
            throw SocketException(std::string("getaddrinfo() failed: ") + gai_strerror(rc));
        bool connected = false;
        for (auto* p = res; p; p = p->ai_next) {
            int r;
            do {
                r = ::connect(fd_, p->ai_addr, static_cast<socklen_t>(p->ai_addrlen));
            } while (r < 0 && errno == EINTR);
            if (r == 0) { connected = true; break; }
        }
        ::freeaddrinfo(res);
        if (!connected)
            throw SocketException("connect() failed to " + host);
    }

    ssize_t send(const uint8_t* data, size_t len) {
        ssize_t n;
        do {
#ifdef _WIN32
// send
            n = ::send(fd_, reinterpret_cast<const char*>(data), static_cast<int>(len), 0);
#else
            n = ::send(fd_, data, len, MSG_NOSIGNAL);
#endif
        } while (n < 0 && errno == EINTR);
        return n;
    }

    ssize_t recv(uint8_t* buf, size_t len) {
        ssize_t n;
        do {
#ifdef _WIN32
            // recieve
            n = ::recv(fd_, reinterpret_cast<char*>(buf), static_cast<int>(len), 0);
#else
            n = ::recv(fd_, buf, len, 0);
#endif
        } while (n < 0 && errno == EINTR);
        return n;
    }

    bool sendAll(const uint8_t* data, size_t len) {
        size_t sent = 0;
        while (sent < len) {
            ssize_t n = this->send(data + sent, len - sent);
            if (n <= 0) return false;
            sent += static_cast<size_t>(n);
        }
        return true;
    }

    bool sendAll(const std::vector<uint8_t>& data) {
        if (data.empty()) return true;
        return sendAll(data.data(), data.size());
    }
// setReuseAddr() sets the SO_REUSEADDR socket option using setsockopt. It allows your socket to bind to an IP address and port combination even if that port is currently in a TIME_WAIT state from a recently closed connectionz

    void setReuseAddr() {
#ifdef _WIN32
        int opt = 1;
        if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0)
            throw SocketException("setsockopt(SO_REUSEADDR) failed");
#else
        int opt = 1;
        if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
            throw SocketException("setsockopt(SO_REUSEADDR) failed");
#endif
    }
    // setNonBlocking() switches the socket's operational mode between blocking (default) and non-blocking.
    // Preventing thread freezes: By default, socket operations are blocking:
    void setNonBlocking(bool nb) {
#ifdef _WIN32
        u_long mode = nb ? 1 : 0;
        if (::ioctlsocket(fd_, FIONBIO, &mode) != 0)
            throw SocketException("ioctlsocket(FIONBIO) failed");
#else
        int flags = ::fcntl(fd_, F_GETFL, 0);
        if (flags < 0) throw SocketException("fcntl(F_GETFL) failed");
        if (nb) flags |= O_NONBLOCK;
        else    flags &= ~O_NONBLOCK;
        if (::fcntl(fd_, F_SETFL, flags) < 0)
            throw SocketException("fcntl(F_SETFL) failed");
#endif
    }

    socket_t fd()    const { return fd_; }
    bool     valid() const { return !IS_INVALID_SOCKET(fd_); }

    void close() { closefd(); }

private:
    socket_t fd_ = INVALID_SOCKET;
    void closefd() {
        if (!IS_INVALID_SOCKET(fd_)) {
            CLOSE_SOCKET(fd_);
            fd_ = INVALID_SOCKET;
        }
    }
};
