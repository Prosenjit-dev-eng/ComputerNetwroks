# Telnet Protocol — Viva Preparation Guide
> C++17 Implementation · RFC 854/855/857/858/1091 · Computer Networks Lab

---

## 🔷 Section 1: What is Telnet?

**Q: What is Telnet?**
> Telnet is an application-layer protocol (RFC 854) that provides a bidirectional, interactive, text-based communication facility over a TCP/IP connection. It defines the **Network Virtual Terminal (NVT)** — an abstract device that normalises character encoding across different terminal types so that heterogeneous machines can communicate.

**Q: What TCP port does Telnet use?**
> Well-known port **23**. Our implementation uses a configurable port (e.g. 5000) for laboratory safety.

**Q: Why is Telnet considered insecure?**
> All data — including usernames and passwords — travels in **plaintext**. Any party on the network path can sniff credentials with a packet analyser (e.g. Wireshark). SSH (RFC 4251) was designed as the secure replacement.

**Q: What replaced Telnet and why?**
> **SSH (Secure Shell)** — it provides encryption, integrity verification, and public-key authentication.

---

## 🔷 Section 2: Architecture

**Q: Describe the client-server architecture of your implementation.**
> Two independent executables:
> - `telnet_server` — listens for TCP connections, spawns a thread per client, maintains per-client NVT/protocol/session state.
> - `telnet_client` — connects to the server, negotiates options, then multiplexes stdin and the socket using `select()`.

**Q: What are the four logical layers in your implementation?**

| Layer | File(s) | Responsibility |
|---|---|---|
| Socket Layer | `socket.hpp` | POSIX & Winsock2 TCP I/O, `sendAll`, error handling |
| NVT Layer | `nvt.hpp` | CR LF / CR NUL encoding & incremental decoding |
| Telnet Protocol Layer | `telnet_protocol.cpp` | IAC parser, option negotiation, subnegotiation |
| Application Layer | `terminal_session.cpp` | Command interpreter (help/echo/date/pwd/clear/exit) |
| Auth & Server Layer | `telnet_server.cpp` | Multi-user credential verification & session isolation |

**Q: How does user authentication work in your implementation?**
> The server implements a three-state authentication machine:
> 1. `AWAITING_USERNAME`: Server prompts `Username: ` upon connection.
> 2. `AWAITING_PASSWORD`: Server receives username and prompts `Password: `.
> 3. `AUTHENTICATED`: If username matches one of the 4 authorized users (`Prosenjit`, `Sayak`, `Sahid`, `Snehasis`) and the password matches (`Prosenjit123`, `Sayak123`, `Sahid123`, `Snehasis123`), access is granted and the personalized shell prompt (`<User>@telnet> `) is presented.
> If authentication fails 3 times, the server drops the connection to prevent brute-forcing.

**Q: How is password masking (`****`) implemented on the client?**
> When the server sends `Password: `, the client's receive thread detects the password prompt and activates password mode. In password mode, standard console line echoing is disabled. The client reads keystrokes one by one (using `_getch()` on Windows or `termios` raw mode on Linux) and prints an asterisk `*` for each character typed. When the user presses Enter, a newline is printed and the complete password is sent over the connection.

**Q: Why is each layer kept independent?**
> **Separation of concerns** — the NVT encoder knows nothing about sockets; the protocol parser knows nothing about terminal commands. This makes each component unit-testable in isolation and replaceable.

---

## 🔷 Section 3: Network Virtual Terminal (NVT)

**Q: What is the NVT?**
> The Network Virtual Terminal is an imaginary, canonical terminal device defined by RFC 854. Both client and server translate between their local representation and NVT format before sending, ensuring interoperability between different OS/terminal combinations.

**Q: What is CR LF in NVT context?**
> On the NVT wire, a **newline** is represented as CR (0x0D) followed by LF (0x0A). A literal carriage-return with no line-feed is sent as CR NUL (0x0D 0x00).

**Q: Why can't you just send `\n` over Telnet?**
> Because different systems use different line endings (Unix: `\n`, Windows: `\r\n`, old Mac: `\r`). NVT mandates CR LF as the universal newline so both sides know exactly how to interpret it.

**Q: What does your NvtEncoder do?**
> - Converts `\r` → `CR LF` (0x0D 0x0A)
> - Escapes `0xFF` bytes → `0xFF 0xFF` (IAC escaping)
> - `encodeLine()` appends `CR LF` at the end

**Q: What does your NvtDecoder do?**
> Statefully decodes the wire stream: `CR LF` → `\n`, `CR NUL` → `\r`. Handles the case where CR arrives in one `recv()` call and LF arrives in the next (TCP fragmentation).

---

## 🔷 Section 4: IAC — Interpret As Command

**Q: What is IAC?**
> IAC = **Interpret As Command**, byte value `0xFF` (255 decimal). When either side receives `0xFF`, it switches from treating bytes as data to treating them as a Telnet command sequence.

**Q: What is the full IAC byte table?**

| Command | Hex | Decimal | Meaning |
|---|---|---|---|
| SE | 0xF0 | 240 | End of subnegotiation |
| NOP | 0xF1 | 241 | No operation |
| DM | 0xF2 | 242 | Data mark |
| BRK | 0xF3 | 243 | Break |
| IP | 0xF4 | 244 | Interrupt process |
| AO | 0xF5 | 245 | Abort output |
| AYT | 0xF6 | 246 | Are you there? |
| EC | 0xF7 | 247 | Erase character |
| EL | 0xF8 | 248 | Erase line |
| GA | 0xF9 | 249 | Go ahead |
| SB | 0xFA | 250 | Subnegotiation begin |
| WILL | 0xFB | 251 | I will use option X |
| WON'T | 0xFC | 252 | I won't use option X |
| DO | 0xFD | 253 | Please use option X |
| DON'T | 0xFE | 254 | Please don't use option X |
| IAC | 0xFF | 255 | Interpret as command |

**Q: How do you send a literal 0xFF byte as data?**
> You escape it: send two consecutive `0xFF` bytes (`IAC IAC`). The receiver's parser recognises `IAC IAC` as a single data byte.

**Q: What is the IAC command structure?**
> - **2-byte**: `IAC <command>` — e.g. `IAC NOP`, `IAC AYT`, `IAC GA`
> - **3-byte**: `IAC <WILL|WONT|DO|DONT> <option>` — negotiation
> - **Variable**: `IAC SB <option> <data...> IAC SE` — subnegotiation

---

## 🔷 Section 5: Option Negotiation

**Q: What is Telnet option negotiation?**
> A four-command handshake (RFC 855) allowing client and server to agree on optional features. Either side can propose enabling or disabling any option.

**Q: What are the four negotiation commands?**

| Command | Sent by | Meaning |
|---|---|---|
| WILL | Either | "I want to enable option X on my side" |
| WON'T | Either | "I refuse to enable option X on my side" |
| DO | Either | "I want you to enable option X on your side" |
| DON'T | Either | "I want you to disable option X on your side" |

**Q: Give an example of ECHO negotiation.**
```
Server → Client:  IAC WILL ECHO     (I will echo your keystrokes)
Client → Server:  IAC DO ECHO       (OK, please do)
```
After this, the server echoes client input back.

**Q: What is the SUPPRESS-GO-AHEAD option?**
> RFC 858. Historically, Telnet used half-duplex with a GA signal to indicate "your turn to send." SGA suppresses this, enabling full-duplex mode — essential for interactive sessions.

**Q: How do you prevent infinite negotiation loops?**
> Using the **RFC 1143 "Q-method"** state machine. Each option has a state: `NO`, `YES`, `WANTYES`, `WANTNO`. Before sending a request, we check the current state — if already `YES` or `WANTYES`, we don't send again.

**Q: What happens when the server receives a DO for an unsupported option?**
> The server must respond with `WON'T <option>` — it explicitly rejects the option. Silently ignoring it would leave the other side waiting indefinitely.

---

## 🔷 Section 6: Subnegotiation

**Q: What is subnegotiation?**
> For options that require more than a yes/no answer, subnegotiation (`IAC SB ... IAC SE`) carries structured parameter data. Example: `TERMINAL-TYPE` requires the client to send the string name of its terminal.

**Q: Describe the TERMINAL-TYPE subnegotiation flow.**
```
Server → Client:  IAC DO TERMINAL-TYPE
Client → Server:  IAC WILL TERMINAL-TYPE

Server → Client:  IAC SB TERMINAL-TYPE SEND IAC SE
Client → Server:  IAC SB TERMINAL-TYPE IS xterm-256color IAC SE
```

**Q: Why must IAC be escaped inside SB data?**
> Because the receiver is scanning for `IAC SE` to find the end. An unescaped `0xFF` would be misinterpreted as the start of a command. So `0xFF` → `0xFF 0xFF` inside SB payload.

---

## 🔷 Section 7: Parser State Machine

**Q: Describe your IAC parser's 5 states.**

| State | Description |
|---|---|
| `DATA` | Normal data accumulation; switch to IAC_RECV on 0xFF |
| `IAC_RECV` | Saw 0xFF; decide next command type |
| `CMD_RECV` | Saw WILL/WONT/DO/DONT; waiting for option byte |
| `SB_RECV` | Inside subnegotiation; accumulating payload bytes |
| `SB_IAC` | Saw IAC inside SB; decide if SE (end) or IAC IAC (escape) |

**Q: Why does the state persist across `recv()` calls?**
> TCP is a **stream** protocol — no message boundaries. A `IAC WILL ECHO` sequence might arrive split: `[IAC]` in one call, `[WILL, ECHO]` in the next. The parser stores its current state so it correctly reassembles multi-byte commands across calls.

---

## 🔷 Section 8: Socket Programming

**Q: What is `sendAll()` and why is it needed?**
> POSIX `send()` may send fewer bytes than requested (partial write). `sendAll()` loops, advancing the pointer, until all bytes are sent or an error occurs. Without this, messages would be silently truncated.

**Q: Why do you handle `EINTR` in recv/send?**
> `EINTR` means the system call was interrupted by a signal. It is not a real error — the correct response is to retry the call.

**Q: What is `SO_REUSEADDR`?**
> After a server restarts, the OS holds the port in TIME_WAIT for ~60 seconds. `SO_REUSEADDR` allows rebinding immediately so the server can restart without delay.

**Q: What is `MSG_NOSIGNAL`?**
> Writing to a disconnected socket sends `SIGPIPE` to the process (terminating it by default). `MSG_NOSIGNAL` suppresses this and returns `EPIPE` as an error code instead.

---

## 🔷 Section 9: Concurrency

**Q: How does your server handle multiple clients?**
> **Thread-per-client model**: `accept()` returns a new socket, the server spawns `std::thread`. Each thread owns a `ClientState` with its own `Socket`, `TelnetProtocol`, `NvtDecoder`, `NvtEncoder`, and `TerminalSession` — no shared mutable state.

**Q: How does the client handle concurrent I/O?**
> Main thread: `sendLoop()` uses `select()` on stdin. Background thread: `recvLoop()` uses `select()` on the socket. Both run simultaneously so the user can type while receiving server output.

---

## 🔷 Section 10: Security Considerations

> **WARNING: Telnet is plaintext — educational use only!**

| Risk | Explanation |
|---|---|
| Plaintext data | Username, password, all data visible on network |
| No server authentication | Cannot verify server identity |
| No encryption | Any intermediary can read and modify the stream |
| Session hijacking | Attacker can inject commands mid-session |
| No `system()` injection | Our server never executes shell commands with client input |

---

## 🔷 Key RFC Numbers

| RFC | Topic |
|---|---|
| RFC 854 | Telnet Protocol Specification |
| RFC 855 | Telnet Option Specification |
| RFC 857 | ECHO Option |
| RFC 858 | SUPPRESS-GO-AHEAD Option |
| RFC 1091 | TERMINAL-TYPE Option |
| RFC 1143 | Q-Method Negotiation Loop Prevention |
