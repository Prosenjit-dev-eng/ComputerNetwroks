# Computer Networks Laboratory Report: Assignment 4

---

## 1. Title Information

- **Course Code**: CSE/PC/B/S/314
- **Course Title**: Computer Networks Laboratory
- **Assignment Number**: Assignment 4
- **Assignment Title**: Implementation of Code Division Multiple Access (CDMA) with Walsh Code
- **Problem Statement**:
  > *"Implement CDMA for multiple access of a common channel by $n$ stations using the same sender-receiver design as previous assignments. Each sender uses a unique code word, given by the Walsh set, to encode its data, send it across the channel, and then perfectly reconstruct the data at $n$ stations."*
- **Deadline**: 05 – 09 October 2026
- **Submission Date**: Next Lab Class (October 2026)
- **Academic Year / Semester**: 3rd Year, 1st Semester (2026–2027)

---

## 2. Design

### 2.1 Purpose of the Program
The purpose of this program is to design, implement, and analyze a software emulation of a **Code Division Multiple Access (CDMA)** network environment. In this environment, $n$ stations simultaneously transmit digital data over a shared physical communication channel without collision, interference, or contention loss. 

To achieve multi-access without time-slotting (TDMA) or frequency subdivision (FDMA), each station is assigned a mutually orthogonal chipping code from a recursively generated **Hadamard-Walsh matrix** of order $N = 2^k \ge n$. Data streams are encapsulated into IEEE 802 standard frames with 32-bit Cyclic Redundancy Check (CRC-32) Frame Check Sequences. The frame bits are modulated into high-frequency chip sequences, linearly superimposed across the common medium, and decoded in parallel at receiver nodes through normalized inner-product correlators. The program evaluates mathematical orthogonality, bit-for-bit reconstruction accuracy, frame integrity, processing gain, and resilience against additive channel noise (AWGN).

### 2.2 Procedural Organization & Architecture Diagram

The software architecture consists of two complementary execution paradigms:
1. **Physical Shared-Medium Mode**: A dedicated TCP channel server (`channel_server.exe`) that listens on port 5050 and interconnects $n$ concurrent station processes (`station.exe`).
2. **All-in-One Multi-Threaded Simulation Mode**: An all-in-one executable (`simulation.exe`) that orchestrates the shared channel and $n$ station threads within a unified console process using native Win32 thread synchronization.

```text
+-----------------------------------------------------------------------------------+
|                           CDMA NETWORK SYSTEM ARCHITECTURE                        |
+-----------------------------------------------------------------------------------+
                                          |
                      +-------------------+-------------------+
                      |                                       |
                      v                                       v
         +-------------------------+             +-------------------------+
         |    Station Node 0       |             |    Station Node (n-1)   |
         |  Payload: "Message 0"   |             | Payload: "Message n-1"  |
         +-------------------------+             +-------------------------+
                      |                                       |
                      v                                       v
         +-------------------------+             +-------------------------+
         | IEEE 802 Frame Builder  |             | IEEE 802 Frame Builder  |
         | MAC + Len + CRC-32 FCS  |             | MAC + Len + CRC-32 FCS  |
         +-------------------------+             +-------------------------+
                      |                                       |
                      v                                       v
         +-------------------------+             +-------------------------+
         |  Walsh Spreader (C_0)   |             | Walsh Spreader (C_n-1)  |
         |   S_0 = d_0 * C_0       |             |  S_n-1 = d_n-1 * C_n-1  |
         +-------------------------+             +-------------------------+
                      \                                       /
                       \                                     /
                        v                                   v
         +-----------------------------------------------------------------+
         |                 SHARED PHYSICAL MEDIUM (CHANNEL)                |
         |             Linear Adder Superposition: C_comp = SUM(S_i)       |
         |            Optional Additive Noise Injection: C_comp + N(0, s^2) |
         +-----------------------------------------------------------------+
                                          |
                                          | Broadcast Composite Vector
                                          v
                      +-------------------+-------------------+
                      |                                       |
                      v                                       v
         +-------------------------+             +-------------------------+
         | Despreader for C_0      |             | Despreader for C_n-1    |
         | d_0 = (C_comp . C_0) / N|             | d_n-1 = (C_comp . C_n-1)|
         +-------------------------+             +-------------------------+
                      |                                       |
                      v                                       v
         +-------------------------+             +-------------------------+
         | CRC-32 Frame Validation |             | CRC-32 Frame Validation |
         |   Reconstructed Msg 0   |             |  Reconstructed Msg n-1  |
         +-------------------------+             +-------------------------+
```

### 2.3 Input and Output Formats

- **Inputs**:
  - `num_stations`: Number of contending network nodes ($n \ge 2$).
  - `demo_mode`: Execution profile (`1`: Full IEEE 802 Frames with CRC-32, `2`: Step-by-Step Mathematical Matrix Walkthrough, `3`: AWGN Noise Stress Test, `4`: Silent / Idle Station Contention Test).
  - `snr_dB`: Target Signal-to-Noise Ratio (dB) for noise stress simulation.
  - Station Payloads: Variable-length ASCII strings (e.g. `"Hello from Station 0"`).
- **Outputs**:
  - $N \times N$ Hadamard-Walsh code assignment table.
  - Pairwise cross-correlation orthogonality verification status (`PASS` / `FAIL`).
  - Bitstream-to-chip vectors and composite medium voltage vectors.
  - Per-station decoded bits, CRC-32 Frame Check Sequence verification (`PASS [OK]`), and recovered ASCII plaintext.
  - Performance Summary Table: Bit Error Rate (BER), Total Chips Transmitted, Delivery Accuracy, Processing Gain ($10 \log_{10}N$ dB), and Execution Latency.

---

## 3. Implementation

### 3.1 Modular Decomposition
The project is partitioned into cohesive, loosely coupled C++ and Python modules:
- [`cdma.h`](file:///c:/Users/hawla/Desktop/CN/Ass4/cdma.h) & [`cdma.cpp`](file:///c:/Users/hawla/Desktop/CN/Ass4/cdma.cpp): Core CDMA algorithms including Hadamard-Walsh matrix generation, orthogonality validation, bit-to-bipolar mapping, chip spreading, linear superposition, and correlator despreading.
- [`frame.h`](file:///c:/Users/hawla/Desktop/CN/Ass4/frame.h) & [`frame.cpp`](file:///c:/Users/hawla/Desktop/CN/Ass4/frame.cpp): IEEE 802 standard frame encapsulation (Source/Dest MAC, Length, Sequence #, Payload padding, and standard CRC-32 FCS calculation).
- [`socketCommon.h`](file:///c:/Users/hawla/Desktop/CN/Ass4/socketCommon.h) & [`socketCommon.cpp`](file:///c:/Users/hawla/Desktop/CN/Ass4/socketCommon.cpp): Windows Sockets (Winsock `ws2_32`) lifecycle management.
- [`channel_server.cpp`](file:///c:/Users/hawla/Desktop/CN/Ass4/channel_server.cpp): Standalone TCP server representing the physical channel bus.
- [`station.cpp`](file:///c:/Users/hawla/Desktop/CN/Ass4/station.cpp): Contending station executable acting as transmitter and multi-station receiver.
- [`simulation.cpp`](file:///c:/Users/hawla/Desktop/CN/Ass4/simulation.cpp): All-in-one multi-threaded simulation engine.
- [`simulation_analysis.py`](file:///c:/Users/hawla/Desktop/CN/Ass4/simulation_analysis.py): Python analytical simulation harness for BER curves, capacity scaling, and figure generation.

### 3.2 Core Code Snippets & Mathematical Methods

#### 1. Recursive Hadamard-Walsh Code Generation
```cpp
vector<vector<int>> generateWalshMatrix(int order) {
    int n = nextPowerOf2(order);
    vector<vector<int>> h = {{1}};
    while ((int)h.size() < n) {
        int k = (int)h.size();
        vector<vector<int>> nextH(2 * k, vector<int>(2 * k, 0));
        for (int r = 0; r < k; r++) {
            for (int c = 0; c < k; c++) {
                nextH[r][c]         =  h[r][c];
                nextH[r][c + k]     =  h[r][c];
                nextH[r + k][c]     =  h[r][c];
                nextH[r + k][c + k] = -h[r][c];
            }
        }
        h = nextH;
    }
    return h;
}
```

#### 2. Spreading and Bitstream Modulation
```cpp
vector<int> encodeBitstream(const string &bitstream, const vector<int> &walshCode) {
    int n = (int)walshCode.size();
    vector<int> chips;
    chips.reserve(bitstream.length() * n);
    for (char b : bitstream) {
        int bitVal = (b == '1') ? +1 : -1;
        for (int k = 0; k < n; k++) {
            chips.push_back(bitVal * walshCode[k]);
        }
    }
    return chips;
}
```

#### 3. Channel Superposition (Vector Summation)
```cpp
vector<int> superimposeSignals(const vector<vector<int>> &stationChips) {
    size_t maxLen = 0;
    for (const auto &chips : stationChips) maxLen = max(maxLen, chips.size());
    vector<int> composite(maxLen, 0);
    for (const auto &chips : stationChips) {
        for (size_t i = 0; i < chips.size(); i++) composite[i] += chips[i];
    }
    return composite;
}
```

#### 4. Receiver Correlator Despreading
```cpp
string decodeCompositeSignal(const vector<int> &compositeSignal, const vector<int> &walshCode, int expectedBits) {
    int n = (int)walshCode.size();
    int totalBlocks = (int)compositeSignal.size() / n;
    if (expectedBits > 0 && expectedBits < totalBlocks) totalBlocks = expectedBits;
    string reconstructed = "";
    for (int b = 0; b < totalBlocks; b++) {
        int dotProduct = 0;
        int baseIdx = b * n;
        for (int k = 0; k < n; k++) {
            dotProduct += compositeSignal[baseIdx + k] * walshCode[k];
        }
        reconstructed += (dotProduct > 0) ? '1' : '0';
    }
    return reconstructed;
}
```

---

## 4. Test Cases

| Test Case ID | Test Category | Configuration & Input Data | Expected Behavior | Observed Result | Status |
|:---:|:---|:---|:---|:---|:---:|
| **TC-01** | **Orthogonality Verification** | $W_4$ and $W_8$ matrices evaluated for all $(i, j)$ pairs. | Auto-correlation $C_i \cdot C_i = N$; Cross-correlation $C_i \cdot C_j = 0$ ($\forall i \ne j$). | Auto-correlation $= N$, cross-correlation $= 0$ for all pairs. Zero cross-talk confirmed. | **PASS** |
| **TC-02** | **Basic 2-Station Multiple Access** | $n=2$, Walsh $W_2$: Node 0 bit `1`, Node 1 bit `0`. | Composite signal $[0, 2]$. Receiver 0 decodes `1`, Receiver 1 decodes `0`. | Perfect reconstruction. Normalized inner products: $+1$ and $-1$. | **PASS** |
| **TC-03** | **Standard 4-Station Simultaneous TX** | $n=4$, $N=4$: Distinct text messages in IEEE 802 frames. | Composite length $= 2528$ chips. All 4 frames reconstructed with CRC-32 valid. | Total Bit Errors $= 0$, BER $= 0.0000\%$, CRC-32 FCS verified on all 4 nodes. | **PASS** |
| **TC-04** | **Higher-Order Scaling ($n=8$)** | $n=8$, $N=8$: 8 concurrent station threads. | All 8 stations decode successfully without inter-channel collisions. | All 8 payloads decoded with 100% accuracy and 0 bit errors. | **PASS** |
| **TC-05** | **Silent / Idle Station Handling** | $n=4$: Stations 0, 1, 2 transmitting; Station 3 silent ($d_3 = 0$). | Silent station produces 0 chips/crosstalk; active stations reconstruct perfectly. | Active stations achieved 100% CRC pass; Station 3 identified as idle. | **PASS** |
| **TC-06** | **AWGN Noise Immunity (SNR = 25 dB)** | $n=4$, AWGN added at SNR $= 25\text{ dB}$. | Clean reconstruction with zero bit errors due to spread-spectrum processing gain. | BER $= 0.0000\%$, CRC-32 PASS on all stations. | **PASS** |
| **TC-07** | **Heavy Noise Stress Test (SNR = 2 dB)** | $n=4$, AWGN added at severe SNR $= 2\text{ dB}$. | Bit errors occur when noise exceeds correlator threshold; CRC flags corrupted frames. | CRC-32 successfully detected and flagged corrupted bits. | **PASS** |

---

## 5. Results

### 5.1 Empirical Performance Metrics Summary

From trials conducted using `simulation.exe` and `simulation_analysis.py`:

| Parameter / Metric | 2 Stations ($N=2$) | 4 Stations ($N=4$) | 8 Stations ($N=8$) | 16 Stations ($N=16$) |
|:---|:---:|:---:|:---:|:---:|
| **Walsh Spreading Order ($N$)** | 2 chips/bit | 4 chips/bit | 8 chips/bit | 16 chips/bit |
| **Active Senders** | 2 | 4 | 8 | 16 |
| **Collisions on Medium** | **0** | **0** | **0** | **0** |
| **Bits Transmitted per Node** | 632 bits | 632 bits | 632 bits | 632 bits |
| **Total Chips on Medium** | 1264 chips | 2528 chips | 5056 chips | 10112 chips |
| **Bit Errors Encountered** | 0 bits | 0 bits | 0 bits | 0 bits |
| **Bit Error Rate (BER)** | **0.0000%** | **0.0000%** | **0.0000%** | **0.0000%** |
| **CRC-32 FCS Validation** | **100% PASS** | **100% PASS** | **100% PASS** | **100% PASS** |
| **Processing Gain ($G_p$)** | 3.01 dB | 6.02 dB | 9.03 dB | 12.04 dB |
| **Execution Latency** | 2.1 ms | 3.0 ms | 5.8 ms | 11.4 ms |

### 5.2 Generated Visualizations

Four high-resolution analytical figures were generated by the simulation suite:
1. `cdma_orthogonality_matrix.png`: Heatmap demonstrating the strict identity form of the normalized cross-correlation matrix $(W W^T)/N = I_N$, confirming zero mutual interference.
2. `cdma_signal_waveforms.png`: Multi-panel time-domain waveform illustrating individual station chipping sequences, the composite channel signal, and the restored digital waveform at Receiver 0.
3. `cdma_ber_vs_snr.png`: Waterfall curves illustrating Bit Error Rate as a function of SNR across varying spreading factors ($N=4, 8, 16, 32, 64$), demonstrating processing gain shift.
4. `cdma_throughput_scaling.png`: Aggregate channel throughput scaling showing linear capacity growth without contention-based collapse.

---

## 6. Analysis

### 6.1 Collision-Free Simultaneous Transmission
In contrast to carrier-sensing random access protocols (such as 1-Persistent, Non-Persistent, or p-Persistent CSMA studied in Assignment 3), where increasing station counts cause severe contention collapse, CDMA achieves simultaneous transmission with **zero collisions**. The aggregate network throughput increases linearly with the number of active users $n \le N$ without degraded packet delivery rates.

### 6.2 Spread Spectrum Processing Gain & Noise Tolerance
Because each bit is spread over $N$ orthogonal chips, the correlator integrates signal energy across the entire chip sequence while averaging out zero-mean uncorrelated noise. The theoretical processing gain:
$$G_p = 10 \log_{10}(N) \text{ dB}$$
allows CDMA to recover transmissions even when the signal power per chip is lower than ambient thermal noise.

### 6.3 Synchronization Requirement & Practical Limitations
- **Synchronous vs Asynchronous Channels**: Walsh codes are strictly orthogonal only when chip transitions arrive in perfect time alignment ($\Delta t = 0$). In asynchronous networks (e.g. mobile uplink), non-zero delays cause inter-code cross-correlation, necessitating Pseudo-Noise (PN) or Gold sequences.
- **The Near-Far Problem**: In wireless environments, if one station is substantially closer than others, its received voltage can overpower orthogonal cancellations unless tight Transmit Power Control (TPC) is applied.

---

## 7. Comments & Evaluation

### 7.1 Learning Outcomes
Through this assignment, we gained a concrete practical understanding of:
1. The mathematical elegance of Sylvester-Hadamard matrices and the mechanics of orthogonal spread-spectrum communication.
2. The fundamental architectural shift from temporal/spectral channel separation (TDMA/FDMA) to mathematical code separation (CDMA).
3. The end-to-end integration of Data Link Layer framing (IEEE 802 header, payload padding, and 32-bit CRC) with Physical Layer chip modulation.

### 7.2 Assignment Assessment
The difficulty level was well-balanced. Designing the software to preserve the modular sender-receiver structure from earlier assignments while incorporating multi-process socket coordination and thread-safe superposition provided an enriching systems programming challenge.

### 7.3 Suggested Improvements
For future lab extensions, incorporating asynchronous timing offsets ($\Delta t \ne 0$) alongside Gold code sequences or implementing an adaptive closed-loop power control algorithm would further bridge textbook CDMA theory with real-world cellular standards (e.g. IS-95 / 3G CDMA2000).
