# Assignment 3: CSMA Network Environment Emulation

## 📦 Reused Components from Ass1 & Ass2
All core files from `Ass1` and `Ass2` have been linked into `Ass3/`:
- [frame.h](file:///c:/Users/hawla/Desktop/CN/Ass3/frame.h) / [frame.cpp](file:///c:/Users/hawla/Desktop/CN/Ass3/frame.cpp): Exact IEEE 802 frame model (15-byte Header with 48-bit MACs, 16-bit length, 8-bit seq; variable Payload; 4-byte CRC-32 Tailer FCS).
- [crc.h](file:///c:/Users/hawla/Desktop/CN/Ass3/crc.h) / [crc.cpp](file:///c:/Users/hawla/Desktop/CN/Ass3/crc.cpp): CRC-8, CRC-10, CRC-16, and CRC-32 algorithms.
- [checkSum.h](file:///c:/Users/hawla/Desktop/CN/Ass3/checkSum.h) / [checkSum.cpp](file:///c:/Users/hawla/Desktop/CN/Ass3/checkSum.cpp): 16-bit one's complement Internet checksum.
- [channel.h](file:///c:/Users/hawla/Desktop/CN/Ass3/channel.h) / [channel.cpp](file:///c:/Users/hawla/Desktop/CN/Ass3/channel.cpp) & [errorInjection.h](file:///c:/Users/hawla/Desktop/CN/Ass3/errorInjection.h) / [errorInjection.cpp](file:///c:/Users/hawla/Desktop/CN/Ass3/errorInjection.cpp): Channel disturbance models.
- [socketCommon.h](file:///c:/Users/hawla/Desktop/CN/Ass3/socketCommon.h) / [socketCommon.cpp](file:///c:/Users/hawla/Desktop/CN/Ass3/socketCommon.cpp): Socket initialization and networking helpers.

---

## 🚀 Multithreaded Full-Duplex CSMA/CD Architecture

### 1. In [client.cpp](file:///c:/Users/hawla/Desktop/CN/Ass3/client.cpp):
- **Full-Duplex Background Listener Thread (`std::thread`)**:
  - Continuously calls `recv()` on the TCP socket connected to port `5050`.
  - When the server detects an overlapping transmission and alerts with `COLLISION`, the listener thread immediately updates the shared atomic flag:
    ```cpp
    g_collision_detected.store(true);
    ```
- **CSMA/CD Chunk-by-Chunk Transmission (Main Thread)**:
  - Senses the channel. When `IDLE`, sends `TX_START`.
  - Streams the frame bitstream chunk-by-chunk in a loop with a small delay (`CHUNK_DELAY_MS`) to emulate bit propagation time.
  - **Mid-Frame Collision Detection**: On every iteration, checks `g_collision_detected.load()`.
  - If `true`:
    - Instantly **aborts** the loop to prevent wasting channel time!
    - Emits a **JAM signal** (`JAM <stationId>`) to warn all stations.
    - Applies **Truncated Binary Exponential Backoff**:
      $$\text{Wait Time} = (\text{rand}() \pmod{2^{\min(k, 10)}}) \times \text{SLOT\_TIME\_MS}$$
    - Retries up to 15 attempts.
  - If the loop finishes without collision, sends `TX_END` and marks the frame **SUCCESSFUL**.

### 2. Supported Transmission Strategies:
- **1-Persistent CSMA**: Senses until `IDLE`, then transmits immediately ($p=1.0$).
- **Non-Persistent CSMA**: If `BUSY`, defers immediately with random exponential backoff without waiting.
- **$p$-Persistent CSMA**: In slotted medium: when `IDLE`, transmits with probability $p$; with $(1-p)$, defers 1 slot and re-senses.
- **CSMA/CD**: Full-duplex listen-while-talk collision detection with early abort and jamming.

### 3. In [server.cpp](file:///c:/Users/hawla/Desktop/CN/Ass3/server.cpp):
- Listens on **Port 5050**.
- Manages shared channel states: `IDLE` (0 stations), `BUSY` (1 station), `COLLISION` (2+ stations).
- When a 2nd station transmits while the channel is `BUSY`:
  - Enters `COLLISION`.
  - Immediately broadcasts `COLLISION\n` to all active transmitters so their background listener threads catch it mid-transmission!
- Upon successful transmission without collision, deserializes the frame (`Frame::deserialize`), verifies the CRC-32 FCS (`verifyCRC`), and logs success.

---

## 🛠️ Compilation & Demo Instructions

### 1-Click Launch
Double-click [run_demo.bat](file:///c:/Users/hawla/Desktop/CN/Ass3/run_demo.bat) inside the `Ass3` directory.

### Manual Compilation
Open a command prompt in `Ass3`:
```cmd
g++ -O2 -std=c++11 server.cpp frame.cpp crc.cpp checkSum.cpp -o server.exe -lws2_32
g++ -O2 -std=c++11 client.cpp frame.cpp crc.cpp checkSum.cpp -o client.exe -lws2_32
```

1. **Terminal 1**: Run `server.exe`
2. **Terminal 2**: Run `client.exe` (Station 1)
3. **Terminal 3**: Run `client.exe` (Station 2)
