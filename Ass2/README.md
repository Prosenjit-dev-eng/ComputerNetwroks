# Assignment 2: Data Link Layer Flow-Control Mechanisms Simulation

## Overview
This project implements and evaluates Data Link Layer flow-control and ARQ (Automatic Repeat reQuest) mechanisms in C++ over UDP sockets in a simulated unreliable network environment.

### Implemented Protocols:
1. **Stop-and-Wait ARQ**: Sender sends 1 frame and waits for its ACK; retransmits on timeout.
2. **Go-Back-N ARQ**: Sender sliding window $N$, Receiver window $1$, cumulative ACKs, retransmits current window on timeout.
3. **Selective Repeat ARQ**: Sender window $N$, Receiver window $N$, selective individual ACKs, out-of-order buffering at receiver, selective retransmission of lost/corrupted frames.

### Key Features:
- **Frame Structure (Ethernet-like)**:
  - Source MAC (6 bytes)
  - Destination MAC (6 bytes)
  - Length (2 bytes)
  - Sequence Number (1 byte)
  - Payload (46–1500 bytes)
  - FCS (4 bytes CRC-32 / CRC-16 / CRC-10 / CRC-8 / Checksum-16 from Assignment 1)
- **Channel Unreliability Simulation**:
  - Packet and ACK drop probability ($p \in [0.1, 0.5]$).
  - Bit error injection (single bit, two bit, odd bit, burst, probabilistic).
  - Random propagation delay and network jitter.
- **Dynamic RTT & Adaptive Timeout (Jacobson & Karn's Algorithm)**:
  - $\text{EstimatedRTT} = (1-\alpha)\text{EstimatedRTT} + \alpha\text{SampleRTT}$
  - $\text{DevRTT} = (1-\beta)\text{DevRTT} + \beta|\text{SampleRTT} - \text{EstimatedRTT}|$
  - $\text{RTO} = \text{EstimatedRTT} + 4\text{DevRTT}$
  - Exponential timer backoff on consecutive timeouts.
- **Performance Evaluation Suite**:
  - Automated benchmark measuring efficiency, throughput, retransmissions, propagation vs ACK reception time.
  - Visualization script generating high-resolution comparison charts.

---

## Compilation & Execution

### 1. Compile Everything (Windows / MinGW g++)
```bash
# Compile Receiver
g++ -std=c++17 receiver.cpp frame.cpp crc.cpp checkSum.cpp errorInjection.cpp socketCommon.cpp channel.cpp -o receiver.exe -lws2_32

# Compile Sender
g++ -std=c++17 sender.cpp frame.cpp crc.cpp checkSum.cpp errorInjection.cpp socketCommon.cpp channel.cpp -o sender.exe -lws2_32

# Compile Benchmark
g++ -O3 -std=c++17 benchmark.cpp frame.cpp crc.cpp checkSum.cpp errorInjection.cpp channel.cpp -o benchmark.exe
```

### 2. Interactive Testing
1. In Terminal 1, run the receiver:
   ```bash
   ./receiver.exe
   ```
2. In Terminal 2, run the sender:
   ```bash
   ./sender.exe
   ```

### 3. Automated Benchmark & Plot Generation
```bash
python evaluate.py
```
This runs the benchmark across all protocols and error rates $[0.0, 0.1, 0.2, 0.3, 0.4, 0.5]$ and generates:
- `evaluation_results.csv`
- `efficiency_vs_error_rate.png`
- `throughput_vs_error_rate.png`
- `retransmissions_vs_error_rate.png`
- `propagation_vs_ack_time.png`
- `overall_comparison_barchart.png`
