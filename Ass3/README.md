# Assignment 3: CSMA Network Environment Emulation (Winsock & Win32)

A multithreaded network simulation and emulation project in C++ simulating **Carrier Sense Multiple Access (CSMA)** protocols and **CSMA with Collision Detection (CSMA/CD)** on a shared half-duplex medium over TCP port 5050.

---

## 📁 Project Structure

```text
Ass3/
├── simulation.cpp              # All-in-One Threaded Simulation (for-loop station threads, single terminal)
├── server.cpp                  # Shared Channel Server (Channel state: IDLE, BUSY, COLLISION; broadcast)
├── client.cpp                  # Contending Station Client (Concurrent Win32 listener, CSMA schemes, Backoff)
├── frame.h / frame.cpp         # Inherited IEEE 802 Frame (15B Header, >=46B Payload, 32-bit CRC FCS)
├── socketCommon.h / .cpp       # Winsock initialization and socket lifecycle helpers
├── run_demo.bat                # Interactive 1-click Windows batch launcher (live demos & experiments)
├── simulation_analysis.py      # Discrete-event statistical simulation harness for Experiments (i) & (ii)
├── p_persistent_analysis.png   # High-resolution plots for Experiment (i) (p-Persistent vs p)
├── csma_comparison_analysis.png# High-resolution 4-panel comparison for Experiment (ii) (vs N)
├── results.csv                 # Detailed empirical trial records from statistical simulation
├── simulation_report.md        # Comprehensive experimental report and performance observations
└── README.md                   # Full documentation and theoretical explanations
```

---

## 🚀 Quick Run: All-in-One Single Terminal Mode (`simulation.exe`)

If you want to run the entire simulation with $N$ stations **in a single terminal** without opening separate command prompt windows:

```cmd
# 1. Compile:
g++ -O2 -std=c++11 simulation.cpp frame.cpp socketCommon.cpp -o simulation.exe -lws2_32

# 2. Run with N stations:
simulation.exe [num_stations] [strategy] [num_frames] [prob_p] [port]

# Examples:
simulation.exe 5 4 3           # 5 stations, CSMA/CD, 3 frames each
simulation.exe 3 1 2           # 3 stations, 1-Persistent, 2 frames each
simulation.exe 4 3 2 0.3       # 4 stations, p-Persistent (p=0.3), 2 frames each
```
This program uses a `for` loop to launch all $N$ station threads concurrently, tracks **frame drops count**, **collision drops**, and **total collisions**, and outputs a complete per-station and aggregate performance report in that same terminal.

---

## 1. Project Architecture

The architecture separates the physical layer channel from contending network stations using a multi-process client-server design:

```text
                       SHARED PHYSICAL MEDIUM
                         (server.exe : 5050)
                      [State: IDLE/BUSY/COLLISION]
                       /          |          \
           TCP Socket /           |           \ TCP Socket
                     v            v            v
                +----------+ +----------+ +----------+
                |Station 1 | |Station 2 | |Station N |
                |client.exe| |client.exe| |client.exe|
                +----------+ +----------+ +----------+
```

1. **Shared Channel (`server.exe`)**: Emulates the shared broadcast transmission bus (e.g., coaxial cable or wireless spectrum). Maintains an active transmitter count `activeTransmitters` to model medium states.
2. **Contending Stations (`client.exe`)**: Independent processes running concurrently, each representing a network node contending for medium access.

---

## 2. How the Shared Channel Works

The server tracks the number of simultaneously transmitting stations (`activeTransmitters`):

$$\text{Channel State} = \begin{cases} \text{IDLE} & \text{if } activeTransmitters = 0 \\ \text{BUSY} & \text{if } activeTransmitters = 1 \\ \text{COLLISION} & \text{if } activeTransmitters \ge 2 \end{cases}$$

- **Carrier Sensing**: When a station sends `SENSE`, the server replies `IDLE` (if 0) or `BUSY` (if $\ge 1$).
- **Transmission Onset**: When a station sends `TX_START`, `activeTransmitters` increments. If `activeTransmitters == 1`, the channel becomes `BUSY`.
- **Immediate Collision Broadcast**: If `activeTransmitters >= 2`, medium contention occurs. The server transitions to `COLLISION` and **immediately broadcasts `"COLLISION\n"`** to all actively transmitting sockets so stations abort early mid-frame.
- **Medium Release**: When a station finishes (`TX_END`) or aborts (`JAM`), `activeTransmitters` decrements. When it reaches 0, the channel returns to `IDLE`.
- **Integrity Validation**: On successful collision-free transmission, the server deserializes the IEEE 802 frame and verifies 32-bit CRC (FCS) validity.

---

## 3. Why the Station Uses a Concurrent Listener Thread

CSMA/CD operates on a **shared half-duplex medium where a station senses the channel while transmitting and detects a collision if another station transmits simultaneously**.

If a station were implemented as a single-threaded blocking sender, it could not read incoming socket packets from the server while executing its transmission loop. 

To overcome this, each station employs a concurrent two-thread architecture:

```text
                           STATION NODE
                                 |
              +------------------+------------------+
              |                                     |
      TX / Main Thread                     RX / Listener Thread
              |                                     |
     - Senses channel                       - Continuously calls recv()
     - Sends TX_START                       - Receives broadcast notifications
     - Streams frame chunks                 - On receiving "COLLISION":
     - Checks g_collision_detected              sets g_collision_detected = true!
              |                                     |
              +------------------+------------------+
                                 |
                     std::atomic<bool> flag
                                 |
                                 v
                 Mid-transmission early abort
                                 |
                                 v
                      Broadcast JAM signal
                                 |
                                 v
                     Truncated Binary Backoff
```

Because the listener thread runs concurrently in the background via Win32 `CreateThread()`, the transmitter detects medium collisions **immediately mid-frame without waiting for the full frame to finish**.

---

## 4. How Each CSMA Scheme Works

### A. 1-Persistent CSMA
- Senses the channel.
- If **BUSY**: Continuously senses until the channel becomes **IDLE**.
- If **IDLE**: Transmits immediately with probability $p = 1.0$.
- Transmits full frame. If collision occurs at the conclusion, backs off and retries.

### B. Non-Persistent CSMA
- Senses the channel.
- If **IDLE**: Transmits immediately.
- If **BUSY**: Does **not** persistently wait; immediately defers by executing random backoff, then senses again. This reduces collisions under high load at the cost of higher idle delay.

### C. $p$-Persistent CSMA
- Operates in slotted time.
- Senses channel until **IDLE**.
- When IDLE: Generates a random number $r \in [0, 1]$:
  - If $r < p$: Transmits immediately.
  - If $r \ge p$: Defers by 1 slot time (`SLOT_TIME_MS`), then re-senses.
- Strikes a balance between 1-Persistent (fast access) and Non-Persistent (collision avoidance).

### D. CSMA/CD (Carrier Sense Multiple Access with Collision Detection)
- 1-Persistent sensing until medium is IDLE.
- Transmits frame in discrete chunks while actively monitoring `g_collision_detected`.
- If collision occurs mid-frame:
  1. Instantly **aborts** further transmission.
  2. Transmits a 32-bit **JAM sequence** (`JAM <stationId>`) to warn all nodes and clear the medium.
  3. Increments collision counter $k$.
  4. Applies **Truncated Binary Exponential Backoff**.
  5. Retries sensing and transmission.

---

## 5. How JAM and Exponential Backoff Work

### JAM Signal
When a station detects overlapping transmissions, it emits a 32-bit Jamming sequence (`JAM`). This guarantees that all contending stations register the collision and abort their transmissions cleanly.

### Truncated Binary Exponential Backoff
To prevent colliding stations from retransmitting simultaneously:
1. Contention Window:
   $$W = 2^{\min(k, 10)} - 1$$
   where $k$ is the collision attempt count (capped at $10$, corresponding to $W = 1023$).
2. Random Slot Selection:
   $$r \in [0, W]$$
3. Backoff Delay:
   $$\text{Delay (ms)} = r \times \text{SLOT\_TIME\_MS}$$
4. The station sleeps for this backoff duration before re-sensing the channel. Retries are allowed up to 15 attempts.

---

## 6. Inherited IEEE 802 Frame Format

The frame structure is inherited from the previous sender-receiver assignments (`frame.h` / `frame.cpp`):
- **Header (15 bytes)**:
  - Source MAC: 6 bytes (48 bits)
  - Destination MAC: 6 bytes (48 bits)
  - Length Field: 2 bytes (16 bits)
  - Sequence Number: 1 byte (8 bits)
- **Payload**: Variable text message padded to minimum 46 bytes (IEEE 802 standard).
- **Tailer (4 bytes)**: 32-bit CRC Frame Check Sequence (FCS).

---

## 7. How to Compile

Run using MinGW GCC on Windows (linking Winsock `ws2_32`):

```cmd
g++ -O2 -std=c++11 server.cpp frame.cpp socketCommon.cpp -o server.exe -lws2_32
g++ -O2 -std=c++11 client.cpp frame.cpp socketCommon.cpp -o client.exe -lws2_32
````

---

## 8. How to Run

### Command-Line Execution
1. **Terminal 1**: Start the server:
   ```cmd
   server.exe 5050
   ```
2. **Terminal 2**: Start Station 1:
   ```cmd
   client.exe 1 5050 csma-cd
   ```
3. **Terminal 3**: Start Station 2:
   ```cmd
   client.exe 2 5050 csma-cd
   ```

#### Client CLI Arguments Syntax:
```cmd
client.exe <station_id> <port> <mode> [p]
```
- `<station_id>`: Unique integer (1, 2, 3, ...)
- `<port>`: TCP port (default: 5050)
- `<mode>`: `csma-cd`, `1-persistent`, `non-persistent`, or `p-persistent`
- `[p]`: Probability for p-persistent (e.g. `0.3`)

---

## 9. How to Use `run_demo.bat`

`run_demo.bat` is a Windows batch utility designed for quick 1-click execution and lab demonstration:

```text
========================================================
      Assignment 3 - CSMA Network Simulator
========================================================

  1. Two-station live demo (CSMA/CD)
  2. Three-station collision demo (High Contention)
  3. Run Python simulation analysis & generate graphs
  4. Compile only (server.exe & client.exe)
  5. Exit
```

- **Why use `run_demo.bat`?**:
  1. Automatically builds both executables and halts if errors occur.
  2. Launches `server.exe` and waits 2 seconds for socket listening to prevent connection refused errors (`WSAECONNREFUSED`).
  3. Spawns stations in separate, titled command prompt windows for side-by-side visual demonstration of concurrent sensing, collision alerts, jam emissions, and backoff.

---

## 10. How to Generate the Analysis Graphs

To run the large-scale statistical experiments and generate high-resolution figures:

```cmd
python simulation_analysis.py
```

### Generated Artifacts:
1. `p_persistent_analysis.png`: 3-panel figure plotting Collisions, Transmission Delay, and Throughput versus probability $p \in [0.1, 0.9]$ for fixed $N = 5$.
2. `csma_comparison_analysis.png`: 4-panel comparison plotting Collisions, Delay, Throughput, and Channel Efficiency versus $N \in [2, 10]$ across all four schemes.
3. `results.csv`: Raw trial data records across all experiments.
4. `simulation_report.md`: Comprehensive experimental report with summary tables, methodology, and performance observations.
