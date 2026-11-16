 Telnet Project — Complete Flow Diagrams

## 1. Full Client–Server Communication Sequence

```mermaid
sequenceDiagram
    participant U as User (stdin)
    participant C as TelnetClient
    participant NET as TCP Stream
    participant S as TelnetServer
    participant TS as TerminalSession

    Note over C,S: Phase 1 — TCP Handshake
    C->>NET: connect(host, port)
    NET->>S: TCP SYN / SYN-ACK / ACK
    S->>S: accept() → spawn client thread

    Note over C,S: Phase 2 — Telnet Option Negotiation
    S->>NET: IAC WILL ECHO (FF FB 01)
    S->>NET: IAC WILL SUPPRESS-GA (FF FB 03)
    S->>NET: IAC DO TERMINAL-TYPE (FF FD 18)
    NET->>C: negotiation bytes arrive

    C->>NET: IAC DO ECHO (FF FD 01)
    C->>NET: IAC WILL SUPPRESS-GA (FF FB 03)
    C->>NET: IAC WILL TERMINAL-TYPE (FF FB 18)
    NET->>S: responses arrive — options confirmed

    S->>NET: IAC SB TERMINAL-TYPE SEND IAC SE
    NET->>C: subnegotiation request arrives
    C->>NET: IAC SB TERMINAL-TYPE IS xterm-256color IAC SE
    NET->>S: terminal type delivered

    Note over C,S: Phase 3 — Interactive Session
    S->>NET: "telnet> " (NVT-encoded prompt)
    NET->>C: prompt bytes
    C->>U: display "telnet> "

    U->>C: user types "help" + Enter
    C->>C: NvtEncoder.encodeLine("help")
    Note right of C: 68 65 6C 70 0D 0A
    C->>NET: encoded bytes sent

    NET->>S: bytes received
    S->>S: TelnetProtocol.feed() → DATA event
    S->>S: NvtDecoder: CR LF → newline
    S->>S: inputBuffer has complete line "help"
    S->>TS: processCommand("help")
    TS->>S: returns help text string

    S->>S: NvtEncoder.encode(response)
    S->>NET: NVT-encoded help text + "telnet> "
    NET->>C: bytes received
    C->>C: TelnetProtocol.feed() → DATA events
    C->>C: NvtDecoder.decode()
    C->>U: display help + prompt

    Note over C,S: Phase 4 — Exit and Disconnect
    U->>C: user types "exit"
    C->>NET: "exit\r\n" NVT-encoded
    NET->>S: bytes
    S->>TS: processCommand("exit")
    TS->>S: shouldExit()=true, "Goodbye!\r\n"
    S->>NET: NVT "Goodbye!\r\n"
    S->>S: close socket, thread exits
    NET->>C: "Goodbye!" then EOF (n=0)
    C->>U: "[Connection closed]"
```

---

## 2. IAC Parser State Machine

```mermaid
stateDiagram-v2
    [*] --> DATA

    DATA --> DATA : regular byte\naccumulate text
    DATA --> IAC_RECV : byte = 0xFF\nflush DATA event

    IAC_RECV --> CMD_RECV : byte = WILL/WONT/DO/DONT\nstore command byte
    IAC_RECV --> SB_RECV : byte = SB (0xFA)\nclear sbBuffer
    IAC_RECV --> DATA : byte = IAC (0xFF)\nemit literal 0xFF as data
    IAC_RECV --> DATA : byte = NOP/AYT/GA/DM...\nemit COMMAND event

    CMD_RECV --> DATA : next byte = option\nemit WILL/WONT/DO/DONT event

    SB_RECV --> SB_RECV : regular byte\nappend to sbBuffer
    SB_RECV --> SB_IAC : byte = IAC (0xFF)

    SB_IAC --> DATA : byte = SE (0xF0)\nemit SUBNEG event
    SB_IAC --> SB_RECV : byte = IAC (0xFF)\nappend literal 0xFF to sbBuffer
    SB_IAC --> CMD_RECV : other byte\nemit partial SUBNEG + new IAC cmd
```

---

## 3. Option Negotiation Q-Method States (RFC 1143)

```mermaid
stateDiagram-v2
    direction LR

    [*] --> NO

    NO --> WANTYES : send WILL or DO\n(first request)
    WANTYES --> YES : peer confirms\nDO (for WILL) or WILL (for DO)
    WANTYES --> NO : peer refuses\nDONT or WONT

    YES --> WANTNO : send WONT or DONT\n(disable request)
    WANTNO --> NO : peer acknowledges

    YES --> YES : duplicate WILL/DO received\nno response (loop prevention)
    NO --> NO : WONT/DONT received\nno action needed
```

---

## 4. Server Threading Architecture

```mermaid
flowchart TD
    A["main(port)"] --> B["TelnetServer(port)\ncreateSocket, bind, listen"]
    B --> C["server.run()"]

    C --> D["select(listenSock, 1s timeout)"]
    D -- timeout --> D
    D -- readable --> E["accept() → new Socket"]
    E --> F["std::thread(handleClient, socket)"]
    F --> D

    subgraph CT["Client Thread (one per connection)"]
        G["ClientState:\nSocket + TelnetProtocol\nNvtDecoder + NvtEncoder\nTerminalSession + inputBuffer"]
        G --> H["sendNegotiation\nWILL ECHO, WILL SGA, DO TERMINAL-TYPE"]
        H --> I["sendPrompt: 'telnet> '"]
        I --> J["sock.recv(buf, 4096)"]
        J -- "n <= 0 (disconnect)" --> K["close / thread exit"]
        J -- "n > 0" --> L["proto.feed(data)\n→ events"]
        L --> M{"event.action?"}
        M -- DATA --> N["nvtDec.decode()\ninputBuffer += text"]
        N --> O{"line complete?"}
        O -- yes --> P["session.processCommand(line)"]
        P --> Q["nvtEnc.encode(response)\nsock.sendAll(response)"]
        Q --> R{"shouldExit?"}
        R -- no --> S["sendPrompt"] --> J
        R -- yes --> K
        O -- no --> J
        M -- "WILL" --> T1["handleWill() → sendAll DO/DONT"] --> J
        M -- "WONT" --> T2["handleWont() → sendAll if needed"] --> J
        M -- "DO" --> T3["handleDo() → sendAll WILL/WONT"] --> J
        M -- "DONT" --> T4["handleDont() → sendAll if needed"] --> J
        M -- "SUBNEG" --> T5["TERMINAL-TYPE IS → record type"] --> J
    end

    F --> CT
```

---

## 5. Client I/O Multiplexing with select()

```mermaid
flowchart LR
    A["run():\nconnect to server\nset supported options\nsendNegotiation"]

    A --> B["spawn recvThread"]
    A --> C["sendLoop (main thread)"]

    subgraph SL["sendLoop — main thread"]
        S1["select(STDIN, 1s)"]
        S1 -- timeout --> S1
        S1 -- stdin ready --> S2["getline(cin, line)"]
        S2 -- EOF --> S3["running=false\nexit"]
        S2 -- line --> S4["nvtEnc.encodeLine(line)"]
        S4 --> S5["sock.sendAll(encoded)"]
        S5 --> S1
    end

    subgraph RL["recvLoop — background thread"]
        R1["select(sock_fd, 1s)"]
        R1 -- timeout --> R1
        R1 -- socket ready --> R2["sock.recv(buf)"]
        R2 -- "n<=0" --> R3["running=false\nConnection closed"]
        R2 -- "n>0" --> R4["proto.feed(data)\n→ events"]
        R4 --> R5{"event.action?"}
        R5 -- DATA --> R6["nvtDec.decode()\ncout << text"]
        R5 -- "WILL/WONT/DO/DONT" --> R7["handleXxx(opt)\nsock.sendAll(response)"]
        R5 -- SUBNEG --> R8["if TERMINAL-TYPE SEND:\nreply IS xterm-256color"]
        R6 & R7 & R8 --> R1
    end

    B --> RL
    C --> SL
```

---

## 6. NVT Encode / Decode Pipeline

```mermaid
flowchart TD
    subgraph SEND["Send Path — NvtEncoder"]
        E1["Local text string"] --> E2{"each character"}
        E2 -- "0x0D (CR)" --> E3["emit 0x0D 0x0A\nCR LF"]
        E2 -- "0xFF (IAC)" --> E4["emit 0xFF 0xFF\nescaped IAC"]
        E2 -- "other" --> E5["emit as-is"]
        E3 & E4 & E5 --> E6["NVT wire bytes\n→ sock.sendAll()"]
    end

    subgraph RECV["Receive Path — NvtDecoder (stateful)"]
        D1["NVT wire bytes\nfrom TelnetProtocol DATA event"] --> D2{"pendingCR_?"}
        D2 -- "no: recv 0x0D" --> D3["pendingCR_=true\nno output yet"]
        D2 -- "no: other" --> D4["emit char"]
        D2 -- "yes: recv 0x0A" --> D5["emit newline\npendingCR_=false"]
        D2 -- "yes: recv 0x00" --> D6["emit CR\npendingCR_=false"]
        D2 -- "yes: recv other" --> D7["emit CR then char\npendingCR_=false"]
        D3 & D4 & D5 & D6 & D7 --> D8["Decoded application text\n→ cout / inputBuffer"]
    end

    E6 -->|"TCP network"| D1
```

---

## 7. Build and Test Pipeline

```mermaid
flowchart TD
    SRC["20 Source Files\n6 headers · 8 src · 4 tests\nCMakeLists.txt"]

    SRC --> CM["cmake -S . -B build\nConfigure: detect compiler\nset C++17, include paths\nregister tests"]

    CM --> BLD["cmake --build build\nCompile all .cpp → .o\nLink telnet_lib.a"]

    BLD --> LIB["telnet_lib.a\nsocket + nvt + telnet_protocol\nterminal_session + server + client"]

    LIB --> EXE1["./build/telnet_server"]
    LIB --> EXE2["./build/telnet_client"]
    LIB --> T1["./build/test_nvt"]
    LIB --> T2["./build/test_protocol"]
    LIB --> T3["./build/test_session"]
    LIB --> T4["./build/integration_test"]

    T1 & T2 & T3 & T4 --> CT["ctest --output-on-failure"]
    CT --> RES{"All tests\npassed?"}
    RES -- yes --> DONE["Ready for Lab Demo"]
    RES -- no --> FIX["Fix compilation/logic errors\nand rebuild"]
    FIX --> BLD

    EXE1 --> DEMO["Lab Demo\nTerminal A: ./telnet_server 5000\nTerminal B: ./telnet_client 127.0.0.1 5000"]
    EXE2 --> DEMO
```
