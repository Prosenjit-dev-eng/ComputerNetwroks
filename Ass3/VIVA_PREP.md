# 📡 Assignment 3 – CSMA Network Emulation: Viva Preparation Guide

> **Topic:** Carrier Sense Multiple Access (CSMA) Variants — 1-Persistent, Non-Persistent, p-Persistent, CSMA/CD
> **Language:** C++ (Win32 API, Winsock2)
> **Architecture:** Multi-process + Multithreaded Client–Server over TCP (Port 5050)

---

## 📋 Table of Contents

1. [Core Concepts](#1-core-concepts)
2. [System Architecture Overview](#2-system-architecture-overview)
3. [Process Flow Diagrams](#3-process-flow-diagrams)
4. [IEEE 802 Frame Structure](#4-ieee-802-frame-structure)
5. [Frame Functions — `frame.cpp`](#5-frame-functions--framecpp)
6. [Server Functions — `server.cpp`](#6-server-functions--servercpp)
7. [Client Functions — `client.cpp`](#7-client-functions--clientcpp)
8. [CSMA Protocol Comparison Table](#8-csma-protocol-comparison-table)
9. [Key Data Structures and Global Variables](#9-key-data-structures-and-global-variables)
10. [Common Viva Questions and Answers](#10-common-viva-questions-and-answers)

---

## 1. Core Concepts

### 1.1 What is CSMA?

**Carrier Sense Multiple Access (CSMA)** is a Media Access Control (MAC) protocol used in shared broadcast networks (e.g., Ethernet, Wi-Fi). Before transmitting, every station **listens (senses) the channel** to check if it is free.

**Core Principle:** *"Listen before you talk."*

CSMA reduces but **does not eliminate** collisions — when two stations sense the channel as IDLE simultaneously and both transmit, their signals overlap and corrupt each other.

---

### 1.2 The Four CSMA Variants Implemented

#### 🔵 1-Persistent CSMA

- **If IDLE** → Transmit **immediately** with probability p = 1.0.
- **If BUSY** → Keep sensing continuously until IDLE, then transmit.
- **Problem:** When multiple stations are all waiting, they ALL transmit the instant the channel goes IDLE — high collision rate.

```
[Want to TX]
     |
Sense Channel
     |
  IDLE? ──YES──> Transmit immediately (p=1.0)
     |
    NO
     |
  [Spin-wait, keep sensing until IDLE]
```

---

#### 🟡 Non-Persistent CSMA

- **If IDLE** → Transmit immediately.
- **If BUSY** → Do **NOT** keep sensing. Instead, wait a **random backoff** time, then sense again.
- **Advantage:** Spreads retransmissions in time → fewer collisions.
- **Disadvantage:** Higher idle time even when channel is free → lower efficiency.

```
[Want to TX]
     |
Sense Channel
     |
  IDLE? ──YES──> Transmit
     |
    NO
     |
  [Random Exponential Backoff]
     |
  [Re-sense channel — repeat from start]
```

---

#### 🟠 p-Persistent CSMA (Slotted)

- Designed for **slotted** channels (time is divided into discrete slots).
- **If BUSY** → Wait until IDLE.
- **If IDLE** → Generate random `r ∈ [0.0, 1.0]`:
  - If `r < p` → Transmit now.
  - If `r ≥ p` → Defer for 1 slot, then re-sense.
- **Purpose:** Tunable balance between aggressive (1-Persistent) and conservative (Non-Persistent).

```
[Want to TX]
     |
Wait until IDLE
     |
Roll r in [0.0, 1.0]
     |
  r < p? ──YES──> Transmit now!
     |
    NO
     |
  Defer 1 slot (60 ms)
     |
  Re-sense
     |
  BUSY? ──YES──> Wait until IDLE again
     |
    NO ──> Roll r again
```

---

#### 🔴 CSMA/CD — Carrier Sense Multiple Access with Collision Detection

- **"Listen while you talk."** The station monitors the channel **during** transmission.
- If a collision is detected **mid-frame** → **immediately abort**.
- Send a **JAM signal** to notify all stations.
- Apply **Truncated Binary Exponential Backoff** and retry.
- Used in **IEEE 802.3 Ethernet**.
- **Key advantage:** Does not waste time finishing a corrupted frame → saves bandwidth.

```
[Want to TX]
     |
Sense Channel
     |
  BUSY? ──YES──> Wait one SLOT_TIME, re-sense
     |
    NO (IDLE)
     |
Send TX_START to server
     |
Stream frame in 16-bit chunks (every 25 ms)
     |
  [After each chunk: check g_collision_detected flag]
     |
  Collision? ──YES──> Abort immediately
     |                     |
    NO               Send JAM signal
     |             Exponential Backoff
  Send TX_END           Retry
     |
Wait for SUCCESS reply
```

---

### 1.3 JAM Signal

- A **32-bit pattern** broadcast immediately after a colliding station aborts its transmission.
- Purpose: Ensures **all stations** on the medium are definitively aware of the collision.
- Without JAM, distant stations might not realize a collision occurred and could mistakenly believe their transmission succeeded.

---

### 1.4 Truncated Binary Exponential Backoff (TBEB)

After the `k`-th collision attempt:

```
Contention Window  W = 2^min(k, 10)
Random slot        r = rand() % W        [r ∈ 0 .. W-1]
Backoff time           = r × SLOT_TIME_MS  (r × 60 ms)
```

| Attempt | Max Window | Max Wait |
|---------|-----------|----------|
| 1st     | 2 slots   | 60 ms    |
| 2nd     | 4 slots   | 180 ms   |
| 5th     | 32 slots  | 1,860 ms |
| 10th+   | 1024 slots| ~61 sec  |

- **Truncated** = window is capped at 2^10 = 1024 slots to prevent unbounded delay.
- After **15 failed attempts** (`MAX_BACKOFF_ATTEMPTS`) → frame is **dropped**.

---

### 1.5 CRC-32 (Frame Check Sequence / Error Detection)

- A **32-bit remainder** computed using modulo-2 polynomial division.
- IEEE 802 standard CRC-32 polynomial (33-bit):
  ```
  100000100110000010001110110110111
  ```
- **Sender:** Appends 32 zeros to data → divides → remainder = FCS → appends to frame.
- **Receiver:** Divides `(data + FCS)` by the same polynomial → if remainder = 0 → **no errors**.

---

## 2. System Architecture Overview

```
┌────────────────────────────────────────────────────────┐
│               SHARED MEDIUM EMULATOR                    │
│              server.exe  (server.cpp)                   │
│                                                         │
│  Listens on TCP Port 5050 (127.0.0.1)                  │
│  Tracks: activeTransmitters  (atomic<int>)              │
│    0  →  IDLE                                           │
│    1  →  BUSY                                           │
│   ≥2  →  COLLISION  → broadcast "COLLISION" to all TX  │
│  Uses: select() — non-blocking I/O multiplexing         │
└──────────────────────┬─────────────────────────────────┘
                       │   TCP connections
          ┌────────────┼────────────┐
          │            │            │
  ┌───────▼──┐  ┌──────▼───┐  ┌───▼──────┐
  │Station 1 │  │Station 2 │  │Station 3 │  ← separate client.exe processes
  │          │  │          │  │          │
  │TX Thread │  │TX Thread │  │TX Thread │  ← main thread: sensing + sending
  │RX Thread │  │RX Thread │  │RX Thread │  ← background: recv() + flag update
  └──────────┘  └──────────┘  └──────────┘
```

**Design Decisions:**
- Each `client.exe` instance = one independent network station.
- Each client has **two concurrent threads**: TX (main) and RX (Win32 `CreateThread`).
- The server is the **shared medium** — it is the arbiter of collisions.
- Communication uses a **newline-delimited text protocol** over TCP.

---

## 3. Process Flow Diagrams

### 3.1 Full CSMA/CD Message Flow (Client ↔ Server)

```
CLIENT TX Thread         SERVER                   CLIENT RX Thread
       |                    |                             |
  sendTcpLine("SENSE") ──► |                             |
                     ◄──── "IDLE" or "BUSY"              |
                            |                     [stores g_channel_state]
                            |                     [SetEvent(g_hSenseEvent)]
  [If IDLE:]                |                             |
  sendTcpLine("TX_START 1") ──► activeTransmitters++      |
                     ◄──── "OK"                           |
                            |                             |
  sendTcpLine("DATA 1 xxxx")──► stationFrameData[1]+=bits |
  (repeat per chunk)        |                             |
                            |  [If Station 2 also sends TX_START:]
                            |  activeTransmitters = 2     |
                            |  Broadcast "COLLISION" ─────────────────────►
                            |                     [g_collision_detected=true]
                            |                     [SetEvent(g_hEndEvent)]
  [TX loop checks flag]     |                             |
  midFrameCollision=true    |                             |
  sendTcpLine("JAM 1") ───► activeTransmitters--          |
                     ◄──── "ABORT_OK"                     |
                            |                             |
  applyExponentialBackoff() |                             |
  [retry from SENSE]        |                             |
                            |                             |
  [No collision path:]      |                             |
  sendTcpLine("TX_END 1") ─► verifyCRC32(bitstream)      |
                     ◄──── "SUCCESS"                      |
                            |                     [g_end_reply_str="SUCCESS"]
                            |                     [SetEvent(g_hEndEvent)]
  [WaitForSingleObject]     |                             |
  reply == "SUCCESS"        |                             |
  Record delivery time      |                             |
```

---

### 3.2 Server Channel State Machine

```
                 [Start]
                    |
              ┌─────▼─────┐
              │   IDLE    │  activeTransmitters = 0
              └─────┬─────┘
                    │ TX_START received
                    ▼
              ┌─────────┐
              │  BUSY   │  activeTransmitters = 1  → reply "OK"
              └──┬──────┘
          ┌──────┤
   TX_END/│    Another TX_START arrives
   JAM    │      │
          │   ┌──▼──────────┐
       [IDLE] │  COLLISION  │  activeTransmitters ≥ 2
              │             │  Broadcast "COLLISION" to all TX stations
              └──┬──────────┘
                 │ Both JAM / TX_END received
                 ▼
              [IDLE]
```

---

### 3.3 Client Internal Thread Architecture

```
┌──────────────────────────────────────────────────┐
│                client.exe Process                 │
│                                                   │
│  ┌─────────────────────────────────┐              │
│  │   Main Thread (TX Logic)        │              │
│  │   senseChannel() → TX_START     │              │
│  │   Stream DATA chunks            │              │
│  │   Check g_collision_detected    │              │
│  │   Send JAM → Backoff → Retry    │              │
│  │   Send TX_END → Wait SUCCESS    │              │
│  └────────────────┬────────────────┘              │
│                   │  shares (lock-free atomics)   │
│  ┌────────────────▼────────────────┐              │
│  │   Shared State                  │              │
│  │   g_collision_detected (atomic) │              │
│  │   g_channel_state (atomic)      │              │
│  │   g_end_reply_str (+ mutex)     │              │
│  │   g_hSenseEvent  (Win32 Event)  │              │
│  │   g_hEndEvent    (Win32 Event)  │              │
│  └────────────────▲────────────────┘              │
│                   │  writes                       │
│  ┌────────────────┴────────────────┐              │
│  │   RX Listener Thread            │              │
│  │   (Win32 CreateThread)          │              │
│  │   Calls recv() continuously     │              │
│  │   Parses: COLLISION, IDLE,      │              │
│  │           BUSY, SUCCESS         │              │
│  │   Updates atomics + Events      │              │
│  └─────────────────────────────────┘              │
└──────────────────────────────────────────────────┘
```

---

## 4. IEEE 802 Frame Structure

Every frame is represented as a **binary bit-string** and has this layout:

```
 Bits:  0        47 48      95 96    111 112  119  120 ....  n-32  n-1
       ┌──────────┬──────────┬────────┬────────┬──────────────┬──────┐
       │  SrcMAC  │ DestMAC  │ Length │  Seq#  │   Payload    │ FCS  │
       │  48 bits │  48 bits │ 16 bits│ 8 bits │  ≥ 368 bits  │ 32 b │
       │  6 bytes │  6 bytes │ 2 bytes│ 1 byte │  ≥ 46 bytes  │ 4 B  │
       └──────────┴──────────┴────────┴────────┴──────────────┴──────┘
       ◄──────────────── Header: 120 bits (15 bytes) ─────────────────►
```

| Field | Size | Value |
|-------|------|-------|
| Src MAC | 48 bits | `00:11:22:33:44:<stationId>` (unique per station) |
| Dest MAC | 48 bits | `FF:FF:FF:FF:FF:FF` (broadcast) |
| Length | 16 bits | Payload size in bytes (unsigned short) |
| Seq# | 8 bits | Frame sequence number (1, 2, 3 …) |
| Payload | ≥ 368 bits | Text message, padded to **min 46 bytes** with `'-'` |
| FCS | 32 bits | CRC-32 computed over Header + Payload |

> **Why 46-byte minimum payload?** IEEE 802.3 requires frames to be long enough that a collision is detectable before the transmitting station finishes. This is tied to the maximum propagation delay across the network.

---

## 5. Frame Functions — `frame.cpp`

### `byteToBinary(unsigned char b)` → `string`
Converts a single byte (0–255) to an 8-character binary string.
```cpp
for (int i = 7; i >= 0; i--)
    res += ((b >> i) & 1) ? '1' : '0';
// byteToBinary(5) → "00000101"
```
**Used for:** encoding each character of the payload, sequence number, MAC bytes.

---

### `binaryToByte(const string& bin)` → `unsigned char`
Inverse of `byteToBinary`. Reconstructs a byte from 8-char binary string using shift-and-OR.
```cpp
val = (val << 1) | (bin[i] - '0');
```

---

### `uint16ToBinary(unsigned short val)` → `string`
Converts a 16-bit integer to 16-char binary string.
**Used for:** the **Length field** in the frame header.

---

### `binaryToUint16(const string& bin)` → `unsigned short`
Inverse of `uint16ToBinary`. Called by `Header::getPayloadLength()` to read the length field.

---

### `textToBinary(const string& text)` → `string`
Converts a text string to binary by calling `byteToBinary` on every character.
```
"Hi" → "01001000" + "01101001" → "0100100001101001"
```
**Used for:** converting the human-readable payload message into bits.

---

### `binaryToText(const string& bin)` → `string`
Inverse of `textToBinary`. Processes 8 bits at a time back into characters.
**Used by:** the **server** to print received payload after successful CRC verification.

---

### `macToBinary(const string& macHex)` → `string`
Converts a MAC address (e.g., `"FF:FF:FF:FF:FF:FF"`) to a 48-bit binary string.
- Strips colons, parses each 2-hex-char byte → converts to 8-bit binary.
- If already a 48-char binary string → returns it unchanged.

---

### `Header::getSequenceNumber()` → `int`
Reads the 8-bit `seqField` binary string and returns it as an integer.
```cpp
return (int)binaryToByte(seqField);
```

---

### `Header::getPayloadLength()` → `int`
Reads the 16-bit `lengthField` and returns the payload size in bytes.
```cpp
return (int)binaryToUint16(lengthField);
```

---

### `Frame::serialize()` → `string`
Concatenates all frame fields into one binary bitstream for transmission:
```cpp
return header.srcMAC + header.destMAC + header.lengthField
     + header.seqField + payload.data + tailer.fcs;
```
This is the string sent chunk-by-chunk in CSMA/CD.

---

### `Frame::getHeaderAndPayload()` → `string`
Returns everything **except** FCS — used as input to CRC verification:
```cpp
return header.srcMAC + header.destMAC + header.lengthField
     + header.seqField + payload.data;
```

---

### `Frame::deserialize(bitstream, fcsBits, outFrame)` → `bool`
Reconstructs a `Frame` object from a raw binary bitstream received by the server:
```
[0..47]    → srcMAC   (48 bits)
[48..95]   → destMAC  (48 bits)
[96..111]  → length   (16 bits)
[112..119] → seqField (8 bits)
[120..n-32]→ payload  (variable)
[n-32..n-1]→ FCS      (32 bits)
```
Returns `false` if bitstream is too short (less than 120 + 32 = 152 bits).

---

### `xorStrings(a, b)` → `string`  *(static)*
Performs bitwise XOR between two binary strings, starting at index 1 (the leading bit is already consumed by the division algorithm).
**Used internally by:** `mod2Div`.

---

### `mod2Div(dividend, divisor)` → `string`  *(static)*
Implements **modulo-2 binary long division** — the mathematical engine behind CRC:
1. Take a window of `divLen` bits from `dividend`.
2. If leading bit = `'1'` → XOR with `divisor`.
3. If leading bit = `'0'` → XOR with zeros (effectively shift).
4. Append next bit of dividend → slide window.
5. Repeat until all bits consumed.
6. Return final remainder.

---

### `computeCRC32(const string& data)` → `string`
Computes the 32-bit FCS:
1. Append **32 zero bits** to data (placeholder for CRC).
2. Call `mod2Div(padded_data, CRC32_POLY)`.
3. Return the 32-bit remainder → this becomes the FCS field.

CRC-32 Polynomial:
```
100000100110000010001110110110111  (33-bit IEEE 802 standard)
```

---

### `verifyCRC32(const string& codeWord)` → `bool`
Verifies received frame integrity:
1. Compute `mod2Div(codeWord, CRC32_POLY)` where `codeWord = data + FCS`.
2. If all bits of remainder are `'0'` → **VALID** (no corruption).
3. Any `'1'` in remainder → **ERROR**.

---

## 6. Server Functions — `server.cpp`

### Setup — `main()` startup sequence
```
initializeSocketSubsystem()            ← WSAStartup
socket(AF_INET, SOCK_STREAM)           ← Create TCP socket
setsockopt(SO_REUSEADDR)               ← Allow port reuse on restart
bind(INADDR_ANY, port 5050)            ← Bind to all interfaces
listen(SOMAXCONN)                      ← Start listening for connections
```

---

### `getChannelStateName(int activeCount)` → `string`
Maps `activeTransmitters` to human-readable state:
```cpp
if (activeCount == 0) return "IDLE";
if (activeCount == 1) return "BUSY";
return "COLLISION";
```

---

### Server Event Loop — `select()`
The server uses **I/O multiplexing** (single-threaded, non-blocking) to handle all clients:
```cpp
fd_set readSet;
FD_ZERO(&readSet);
FD_SET(listenSocket, &readSet);
for (each clientSocket) FD_SET(clientSocket, &readSet);

select(0, &readSet, NULL, NULL, &timeout);  // 15 ms polling timeout
```
- No separate thread per client — `select()` tells which sockets have data ready.
- Efficient for an emulated shared medium where ordering of events matters.

---

### Command: `SENSE`
```
Client sends:  "SENSE\n"
Server checks: activeTransmitters
Server replies: "IDLE\n"  if activeTransmitters == 0
                "BUSY\n"  otherwise
```
Increments `totalSenseCount` for statistics.

---

### Command: `TX_START <stationId>`
```
activeTransmitters++
transmittingStations.insert(stationId)
totalTxAttempts++

if activeTransmitters == 1:
    reply "OK\n"                         ← Channel is BUSY for this station only
else:  // activeTransmitters >= 2 → COLLISION!
    totalCollisions++
    for each station in transmittingStations:
        send "COLLISION\n"               ← Broadcast to ALL transmitters
```

---

### Command: `DATA <stationId> <bitchunk>`
Accumulates incoming bitstream chunks into `stationFrameData[stationId]`.
If the station is already in the `collidedStations` set → re-send `"COLLISION\n"` to ensure the client gets the message.

---

### Command: `JAM <stationId>`
Called when a CSMA/CD client aborts mid-frame:
```
transmittingStations.erase(stationId)
collidedStations.erase(stationId)
stationFrameData.erase(stationId)
activeTransmitters--
reply "ABORT_OK\n"
```

---

### Command: `TX_END <stationId>`
Concludes frame transmission:
```
bool wasCollided = collidedStations.count(stationId) > 0
activeTransmitters--
transmittingStations.erase(stationId)

if wasCollided:
    reply "COLLISION\n"
else:
    totalSuccesses++
    reply "SUCCESS\n"
    Frame::deserialize(bitstream, 32, rxFrame)    ← Parse received frame
    verifyCRC32(rxFrame.getHeaderAndPayload() + rxFrame.tailer.fcs)
    binaryToText(rxFrame.payload.data)            ← Print decoded message
```

---

### Command: `STATS`
Returns aggregate counters:
```
"Attempts=X Success=Y Collisions=Z Senses=W\n"
```

---

## 7. Client Functions — `client.cpp`

### `createIEEE802Frame(stationId, seqNum, rawMessage)` → `Frame`
Assembles a complete IEEE 802 frame:
1. Build `srcMAC` = `"00:11:22:33:44:<stationId>"` → convert to 48-bit binary.
2. Set `destMAC` = `"FF:FF:FF:FF:FF:FF"` (broadcast) → 48-bit binary.
3. Pad `rawMessage` to ≥ 46 bytes with `'-'` characters.
4. Convert all fields to binary (`textToBinary`, `uint16ToBinary`, `byteToBinary`).
5. Compute **CRC-32** over `srcMAC + destMAC + length + seq + payload`.
6. Return assembled `Frame(header, payload, tailer)`.

---

### `socketListenerThread(LPVOID lpParam)` → `DWORD WINAPI`
**Background RX thread** — runs concurrently with the TX main thread:
```cpp
while (!g_stop_listener) {
    recv(sock, buffer, ...)          // Blocking wait for server data
    // Parse received lines:

    if line contains "COLLISION":
        g_collision_detected = true  // Signal TX thread to abort!
        g_end_reply_str = "COLLISION"
        SetEvent(g_hEndEvent)        // Wake TX if waiting for TX_END reply
        SetEvent(g_hSenseEvent)      // Wake TX if waiting for SENSE reply

    if line == "IDLE":
        g_channel_state = 0
        SetEvent(g_hSenseEvent)

    if line == "BUSY":
        g_channel_state = 1
        SetEvent(g_hSenseEvent)

    if line == "SUCCESS":
        g_end_reply_str = "SUCCESS"
        SetEvent(g_hEndEvent)        // Wake TX thread — frame delivered!
}
```
Uses `CRITICAL_SECTION` (`g_reply_cs`) to safely write `g_end_reply_str`.

---

### `sendTcpLine(SOCKET sock, const string& text)` → `bool`
Appends `'\n'` and sends via TCP:
```cpp
string msg = text + "\n";
send(sock, msg.c_str(), msg.length(), 0);
```
All protocol messages are newline-delimited.

---

### `senseChannel(SOCKET sock)` → `string`
Performs carrier sensing:
```cpp
ResetEvent(g_hSenseEvent);          // Clear any stale event
sendTcpLine(sock, "SENSE");         // Ask server
WaitForSingleObject(g_hSenseEvent, 300ms);  // Block until reply arrives
return (g_channel_state == 0) ? "IDLE" : "BUSY";
```
The RX thread receives the server's `"IDLE"` or `"BUSY"` reply and signals `g_hSenseEvent`.

---

### `applyExponentialBackoff(int attempt, int stationId)` → `int`
Truncated Binary Exponential Backoff implementation:
```cpp
int k = min(attempt, 10);           // Cap exponent at 10
int maxSlots = (1 << k);            // 2^k
int randomSlots = rand() % maxSlots;
int backoffMs = randomSlots * SLOT_TIME_MS;   // × 60 ms
Sleep(backoffMs);
return backoffMs;
```

---

### `transmitCsmaCd(sock, stationId, frameSeq, frame, outCollisions, outDelayMs)` → `bool`
**Full CSMA/CD algorithm** (the most important function):

```
Step 1: Sense channel in a loop until IDLE
Step 2: Reset g_collision_detected = false
Step 3: Send "TX_START <id>" to server
Step 4: Loop over frame bitstream in 16-bit chunks:
          Sleep(CHUNK_DELAY_MS)             ← emulate propagation delay
          Send "DATA <id> <chunk>"
          CHECK g_collision_detected flag!
          If true:
              midFrameCollision = true
              outCollisions++
              Send "JAM <id>"
              break out of chunk loop
Step 5: If midFrameCollision:
            applyExponentialBackoff(attempt)
            continue  (retry)
Step 6: If all chunks sent:
            Send "TX_END <id>"
            WaitForSingleObject(g_hEndEvent, 500ms)
            Read g_end_reply_str
            If "SUCCESS" → record delivery time, return true
            If "COLLISION" → backoff, retry
Step 7: After MAX_BACKOFF_ATTEMPTS → drop frame, return false
```

---

### `transmit1Persistent(sock, stationId, frameSeq, frame, ...)` → `bool`
```
Loop (up to MAX_BACKOFF_ATTEMPTS):
    Sense channel in a loop until IDLE (spin-wait)
    Send "TX_START <id> <entire_bitstream>"   ← Full frame in one message
    Sleep(totalBits * 2)                       ← Wait for transmission time
    Send "TX_END <id>"
    Wait for reply
    If COLLISION → backoff, retry
    If SUCCESS   → return true
```

---

### `transmitNonPersistent(sock, stationId, frameSeq, frame, ...)` → `bool`
```
Loop (up to MAX_BACKOFF_ATTEMPTS):
    Sense channel ONCE
    If BUSY → applyExponentialBackoff(), continue  (NO spin-waiting!)
    If IDLE → send full frame → wait for reply
    If COLLISION → backoff, retry
    If SUCCESS   → return true
```

---

### `transmitPPersistent(sock, stationId, frameSeq, p, frame, ...)` → `bool`
```
Loop (up to MAX_BACKOFF_ATTEMPTS):
    Wait until channel is IDLE (spin-sense)
    Inner loop:
        Roll r = rand() / RAND_MAX   ∈ [0.0, 1.0]
        If r < p  → break inner loop (ready to transmit)
        If r >= p → Sleep(SLOT_TIME_MS)  (defer 1 slot)
                    Re-sense:
                      If IDLE  → roll r again
                      If BUSY  → break inner loop, re-enter outer loop
    Send full frame → wait for reply
    If COLLISION → backoff, retry
    If SUCCESS   → return true
```

---

### `main()` — Client Setup
```
InitializeCriticalSection(&g_reply_cs)
CreateEvent(NULL, FALSE, FALSE, NULL) → g_hSenseEvent  (auto-reset)
CreateEvent(NULL, FALSE, FALSE, NULL) → g_hEndEvent    (auto-reset)
initializeSocketSubsystem()           → WSAStartup
socket() + connect(127.0.0.1:5050)   → TCP connection to server
CreateThread(socketListenerThread)    → Launch RX background thread
[Parse CLI args: stationId, port, mode, p, numFrames]
[Transmit numFrames frames using selected strategy]
[Print performance report]
[Cleanup: g_stop_listener=true, shutdown socket, CloseHandle, WSACleanup]
```

---

## 8. CSMA Protocol Comparison Table

| Feature | 1-Persistent | Non-Persistent | p-Persistent | CSMA/CD |
|---------|-------------|----------------|--------------|---------|
| Sense before TX | Yes | Yes | Yes | Yes |
| If channel BUSY | Spin-wait for IDLE | Random backoff immediately | Wait for IDLE | Wait for IDLE slot |
| If channel IDLE | Transmit (p=1.0) | Transmit immediately | Transmit with prob p | Transmit, monitor |
| Mid-TX collision detection | No | No | No | **Yes** |
| JAM signal | No | No | No | **Yes** |
| Collision handling timing | After full frame | After full frame | After full frame | **Mid-frame abort** |
| Slotted channel | No | No | **Yes** | No |
| Backoff after collision | Yes (TBEB) | Yes (TBEB) | Yes (TBEB) | Yes (TBEB) |
| Best for | Light load | Heavy load | Medium/tunable | Wired Ethernet |
| Efficiency | Low | Medium | Tunable | **Highest** |

---

## 9. Key Data Structures and Global Variables

### Client-Side (shared between TX and RX threads)

| Variable | Type | Purpose |
|----------|------|---------|
| `g_collision_detected` | `atomic<bool>` | RX thread sets `true` when COLLISION received; TX thread polls this mid-frame |
| `g_stop_listener` | `atomic<bool>` | Set `true` at cleanup to signal RX thread to exit |
| `g_channel_state` | `atomic<int>` | 0 = IDLE, 1 = BUSY; written by RX, read by TX after SENSE |
| `g_hSenseEvent` | `HANDLE` (Win32 Event, auto-reset) | Signaled by RX when SENSE reply arrives; TX waits on it |
| `g_hEndEvent` | `HANDLE` (Win32 Event, auto-reset) | Signaled by RX when TX_END reply arrives; TX waits on it |
| `g_reply_cs` | `CRITICAL_SECTION` | Mutex protecting `g_end_reply_str` from concurrent access |
| `g_end_reply_str` | `string` | Stores the last TX_END reply: `"SUCCESS"` or `"COLLISION"` |

### Server-Side

| Variable | Type | Purpose |
|----------|------|---------|
| `activeTransmitters` | `atomic<int>` | Count of stations currently transmitting |
| `transmittingStations` | `set<int>` | IDs of stations in active transmission |
| `collidedStations` | `set<int>` | IDs of stations marked as collided (before JAM/TX_END) |
| `stationFrameData` | `map<int,string>` | Accumulated bitstream per station |
| `socketToStationId` | `map<SOCKET,int>` | Maps socket handle → station ID |
| `stationIdToSocket` | `map<int,SOCKET>` | Maps station ID → socket (for broadcasting COLLISION) |
| `clientBuffers` | `map<SOCKET,string>` | Per-client receive buffer for TCP fragmentation handling |

### Protocol Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| `SLOT_TIME_MS` | 60 ms | Base time unit: backoff slot size and slotted CSMA slot duration |
| `CHUNK_DELAY_MS` | 25 ms | Delay between DATA chunks — emulates physical bit propagation |
| `CHUNK_SIZE_BITS` | 16 bits | Bits sent per DATA message in CSMA/CD chunked streaming |
| `MAX_BACKOFF_ATTEMPTS` | 15 | Maximum retransmission attempts before frame is dropped |

---

## 10. Common Viva Questions and Answers

**Q1: What is the difference between CSMA and CSMA/CD?**
> CSMA senses the channel only **before** transmitting — if a collision occurs during transmission, the station wastes time sending a corrupted frame to completion. CSMA/CD adds **collision detection during transmission**: the station immediately aborts on detecting a collision, sends a JAM signal, and retransmits after backoff. This saves significant channel bandwidth.

---

**Q2: Why does the server use `select()` instead of one thread per client?**
> `select()` is single-threaded I/O multiplexing — the server can monitor all client sockets simultaneously without the overhead of thread creation and synchronization. For a shared-medium emulator, this also prevents race conditions since all state updates happen sequentially in one thread.

---

**Q3: What happens when two stations transmit simultaneously?**
> Both send `TX_START`. The server increments `activeTransmitters` to 2 on the second call and immediately broadcasts `"COLLISION\n"` to all transmitting stations. Each client's background RX thread receives this and sets `g_collision_detected = true`. The TX thread detects this flag on the next chunk iteration, aborts, sends `"JAM"`, and applies exponential backoff.

---

**Q4: Why minimum 46-byte payload in IEEE 802.3?**
> The minimum frame size ensures the frame is large enough that a station can detect a collision before it finishes transmitting. This is mathematically tied to the slot time = 2 × maximum propagation delay. A frame smaller than 64 bytes (header + payload + FCS) could finish transmitting before a collision signal from the farthest station arrives.

---

**Q5: What is the purpose of the JAM signal?**
> After detecting a collision, the station sends a 32-bit JAM pattern. This ensures all other stations on the medium hear the collision — they cannot mistake the truncated corrupted data for a valid frame. It enforces that the collision is universally recognized before backoff begins.

---

**Q6: Explain Truncated Binary Exponential Backoff.**
> After the k-th collision, the station picks a random integer `r` from `[0, 2^min(k,10) - 1]` and waits `r × SLOT_TIME` before retrying. The window size doubles with each failure (spreading retransmissions across time). "Truncated" means the window is capped at 2^10 = 1024 to prevent unbounded delay. After 15 failures, the frame is dropped.

---

**Q7: What is `atomic<bool> g_collision_detected` and why is it atomic?**
> It is a shared flag between the TX thread and the RX thread. `atomic<bool>` guarantees that reads and writes are thread-safe without a mutex — no data race. Using a regular `bool` would be undefined behavior in C++ when two threads access it concurrently without synchronization.

---

**Q8: How does p-Persistent differ from 1-Persistent?**
> In 1-Persistent, when idle, the station **always** transmits (p=1) → if multiple stations are waiting, they all transmit simultaneously. In p-Persistent, the station transmits with probability `p` and defers 1 slot with probability `(1-p)` → randomly spreads retransmissions across time, reducing collisions at the cost of slight additional delay.

---

**Q9: What is Non-Persistent CSMA's advantage?**
> It avoids the "thundering herd" problem. Instead of all waiting stations simultaneously transmitting the instant the channel becomes IDLE, each performs a random backoff — naturally spacing out their attempts. Trade-off: the channel may remain idle even when stations have data to send (since they are backing off, not sensing).

---

**Q10: How is CRC-32 verified at the receiver?**
> The sender appended the 32-bit CRC remainder to the frame. The receiver runs the same `mod2Div` algorithm on the entire `(data + FCS)` bit-string using the same polynomial. If no bits were corrupted, the remainder is **all zeros** → frame is valid. Any non-zero remainder indicates a corrupted bit somewhere in the frame.

---

**Q11: What does `WaitForSingleObject(g_hEndEvent, 500)` do?**
> It blocks the TX thread for up to 500 ms, waiting for the `g_hEndEvent` Win32 auto-reset event to be signaled. The RX listener thread signals this event when it receives a `SUCCESS` or `COLLISION` reply from the server after `TX_END`. This avoids busy-polling — the TX thread sleeps until the event fires.

---

**Q12: What performance metrics does the client report?**

| Metric | Computation |
|--------|-------------|
| Frames Delivered | Count of successful `transmit*()` calls returning `true` |
| Frames Dropped | `numFrames − totalDelivered` |
| Total Collisions | Sum of `collisions` across all frames |
| Average Delay per Frame | `totalDelayMs / totalDelivered` |
| Channel Throughput | `totalPayloadBitsDelivered / totalElapsedSeconds` (bps) |
| Channel Efficiency | `(totalDelivered / numFrames) × 100` % |

---

*End of Viva Preparation Guide — Assignment 3: CSMA Network Emulation*
