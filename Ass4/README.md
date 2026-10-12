# Assignment 4: CDMA with Walsh Code (Multi-Station Common Channel Emulation)

**Course**: CSE/PC/B/S/314: Computer Networks Lab  
**Assignment**: Implement CDMA with Walsh Code for Multiple Access of a Common Channel by $n$ Stations  
**Platform**: Windows (Winsock `ws2_32`, Win32 Multithreading, MinGW C++11) & Python 3 (Matplotlib, NumPy, Pandas)

---

## 📁 Project Structure

```text
Ass4/
├── cdma.h / cdma.cpp           # Hadamard-Walsh Generator, Spreading, Superposition & Despreading
├── frame.h / frame.cpp         # IEEE 802 Frame Structure, CRC-32 FCS Calculation & Validation
├── socketCommon.h / .cpp       # Winsock Lifecycle & Network Socket Abstraction
├── simulation.cpp              # All-in-One Multi-Threaded Simulation (Single-Console Runner)
├── channel_server.cpp          # Shared Physical Medium TCP Server (Superimposes Chips)
├── station.cpp                 # Contending Station Node (Transmitter + Concurrent Receiver)
├── simulation_analysis.py      # Statistical Analysis, BER Simulation & High-Res Graph Generation
├── run_demo.bat                # 1-Click Interactive Batch Launcher
├── results.csv                 # Empirical BER vs SNR and Throughput Data Records
├── cdma_orthogonality_matrix.png # Heatmap of Cross-Correlation (0 Cross-Talk Proof)
├── cdma_signal_waveforms.png   # Time-Domain Signal Waveforms & Superposition Diagram
├── cdma_ber_vs_snr.png         # BER vs SNR Waterfall Curves (Processing Gain)
├── cdma_throughput_scaling.png # Linear Capacity & Throughput Scaling vs Active Stations
├── CDMA_REPORT.md              # Full Formal Laboratory Report adhering to Report_guidelines.pdf
├── VIVA_PREP.md                # Exhaustive Viva & Lab Exam Prep (30+ Questions & Answers)
└── README.md                   # Complete Documentation & Execution Guide
```

---

## 🚀 Quick Run Guide

### Option 1: 1-Click Windows Batch Launcher (`run_demo.bat`)
Simply double-click `run_demo.bat` in File Explorer or run it in Command Prompt:
```cmd
run_demo.bat
```
This presents an interactive menu to run all-in-one simulations, educational mathematical demos, standalone multi-process demos in separate windows, or plot generation.

---

### Option 2: All-in-One Simulation (`simulation.exe`) — **Recommended**
Run the complete emulation (shared medium + $n$ station threads) in a **single terminal**:

```cmd
# 1. Compile:
g++ -O2 -std=c++11 simulation.cpp cdma.cpp frame.cpp socketCommon.cpp -o simulation.exe -lws2_32

# 2. Run:
# Syntax: simulation.exe [num_stations] [demo_mode: 1=Frames, 2=Math, 3=Noise, 4=Silent] [snr_dB]

# Example A: Standard 4-station full frame simulation with CRC-32
simulation.exe 4 1

# Example B: Step-by-step mathematical chip & dot product walkthrough
simulation.exe 4 2

# Example C: 8-station high-concurrency simulation
simulation.exe 8 1

# Example D: Additive Gaussian Noise (AWGN) stress test at SNR = 5 dB
simulation.exe 4 3 5.0
```

---

### Option 3: Standalone Multi-Process Mode (`channel_server.exe` + `station.exe`)
Run the physical channel server and individual station processes across multiple terminals:

```cmd
# 1. Compile:
g++ -O2 -std=c++11 channel_server.cpp cdma.cpp frame.cpp socketCommon.cpp -o channel_server.exe -lws2_32
g++ -O2 -std=c++11 station.cpp cdma.cpp frame.cpp socketCommon.cpp -o station.exe -lws2_32

# 2. Terminal 1: Launch Channel Server (e.g. 4 stations on port 5050)
channel_server.exe 5050 4

# 3. Terminals 2, 3, 4, 5: Launch 4 Station Nodes
station.exe Station_0 127.0.0.1 5050 "Hello from Node 0!"
station.exe Station_1 127.0.0.1 5050 "Hello from Node 1!"
station.exe Station_2 127.0.0.1 5050 "Hello from Node 2!"
station.exe Station_3 127.0.0.1 5050 "Hello from Node 3!"
```

---

### Option 4: Python Statistical Analysis & High-Res Plots
Generate publication-quality graphs and `results.csv`:
```cmd
python simulation_analysis.py
```

---

## 🧠 Theory & Mathematical Foundations

### 1. Code Division Multiple Access (CDMA)
In conventional medium access control (MAC) protocols (e.g. TDMA or CSMA):
- Stations either transmit at different times (TDMA / CSMA backoff) or risk destructive collisions when overlapping.
- In **CDMA**, all $n$ stations transmit **simultaneously across the exact same frequency band**. Signals overlap on the physical medium, yet every station's data can be **perfectly separated and reconstructed** using mutually orthogonal spreading codes.

### 2. Sylvester-Hadamard Construction of Walsh Codes
Walsh codes are row vectors from a square Hadamard matrix $W_N$ of order $N = 2^k \ge n$.
The matrices are constructed recursively:
$$W_1 = [1]$$
$$W_{2k} = \begin{bmatrix} W_k & W_k \\ W_k & -W_k \end{bmatrix}$$

For order $N=4$:
$$W_4 = \begin{bmatrix}
+1 & +1 & +1 & +1 \\
+1 & -1 & +1 & -1 \\
+1 & +1 & -1 & -1 \\
+1 & -1 & -1 & +1
\end{bmatrix}$$

### 3. Orthogonality Condition
Two Walsh code vectors $C_i$ and $C_j$ satisfy:
$$\sum_{k=0}^{N-1} C_i[k] \cdot C_j[k] = \begin{cases} N & \text{if } i = j \text{ (Auto-correlation)} \\ 0 & \text{if } i \neq j \text{ (Cross-correlation / Zero Cross-Talk)} \end{cases}$$

### 4. Spreading & Transmission
1. **Bit Mapping**:
   $$\text{Bit } 1 \to d_i = +1, \quad \text{Bit } 0 \to d_i = -1, \quad \text{Silent/Idle } \to d_i = 0$$
2. **Chip Spreading**:
   Station $i$ multiplies its bipolar bit $d_i$ by its Walsh code $C_i$:
   $$S_i = d_i \cdot C_i = [d_i C_i[0], d_i C_i[1], \dots, d_i C_i[N-1]]$$
3. **Common Channel Superposition (Linear Addition)**:
   The physical medium sums all transmitting stations' electromagnetic/electrical signals:
   $$C_{\text{composite}} = \sum_{i=0}^{n-1} S_i = \sum_{i=0}^{n-1} d_i \cdot C_i$$

### 5. Perfect Reconstruction (Despreading at Receivers)
To extract the data transmitted by Station $j$, the receiver computes the normalized inner product of $C_{\text{composite}}$ with $C_j$:
$$\frac{C_{\text{composite}} \cdot C_j}{N} = \frac{1}{N} \left( \sum_{i=0}^{n-1} d_i C_i \right) \cdot C_j = \frac{1}{N} \sum_{i=0}^{n-1} d_i (C_i \cdot C_j)$$
Because $C_i \cdot C_j = 0$ for all $i \ne j$, every other station's signal cancels out completely to zero:
$$\frac{C_{\text{composite}} \cdot C_j}{N} = \frac{1}{N} [d_j (C_j \cdot C_j)] = \frac{1}{N} [d_j \cdot N] = d_j$$

- If result is $+1 \to$ Original bit was `1`.
- If result is $-1 \to$ Original bit was `0`.
- 100% bit-for-bit perfect reconstruction!

---

## 🛡️ Data Link Layer Frame & CRC-32 Validation

To satisfy the assignment's requirement (*"Use the same sender-receiver design as previous assignments"*):
- Every transmission is encapsulated in an **IEEE 802 standard frame**:
  - **Source MAC**: 48 bits (e.g. `00:11:22:33:44:0X`)
  - **Destination MAC**: 48 bits (e.g. `FF:FF:FF:FF:FF:FF`)
  - **Length Field**: 16 bits
  - **Sequence Number**: 8 bits
  - **Payload**: User text message (padded to $\ge 46$ bytes standard minimum payload)
  - **Tailer (FCS)**: 32-bit CRC using IEEE 802.3 polynomial ($0x04C11DB7$)
- Upon reception and despreading, each station verifies the CRC-32 Frame Check Sequence. A 100% match verifies that spreading, medium superposition, and despreading preserved total frame integrity.

---

## 📊 Summary of Experimental Results

| Metric | 4 Stations ($N=4$) | 8 Stations ($N=8$) | 16 Stations ($N=16$) |
|:-------|:------------------:|:------------------:|:--------------------:|
| **Walsh Code Order ($N$)** | 4 chips/bit | 8 chips/bit | 16 chips/bit |
| **Simultaneous Senders** | 4 | 8 | 16 |
| **Medium Collisions** | **0** | **0** | **0** |
| **Bit Error Rate (BER)** | **0.0000%** | **0.0000%** | **0.0000%** |
| **Frame Delivery Rate** | **100.0%** | **100.0%** | **100.0%** |
| **CRC-32 FCS Integrity** | **PASS [OK]** | **PASS [OK]** | **PASS [OK]** |
| **Theoretical Processing Gain** | 6.02 dB | 9.03 dB | 12.04 dB |
