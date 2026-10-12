# Assignment 4: CDMA with Walsh Code — Viva & Lab Exam Prep Guide

**Course**: CSE/PC/B/S/314: Computer Networks Lab  
**Topic**: Code Division Multiple Access (CDMA) with Walsh/Hadamard Codes

---

## 📌 Section 1: Fundamental Principles & Definitions

### Q1: What is CDMA and how does it fundamentally differ from TDMA and FDMA?
- **FDMA (Frequency Division Multiple Access)**: Divides the total available bandwidth into non-overlapping frequency channels. Each station is assigned a fixed frequency band and transmits at all times within that band.
- **TDMA (Time Division Multiple Access)**: Divides time into synchronized periodic frames and slots. All stations share the full frequency bandwidth, but each station transmits only within its dedicated time slot.
- **CDMA (Code Division Multiple Access)**: All stations transmit **simultaneously across the entire frequency band at all times**. Contention is avoided not by frequency or time division, but through **code division**: each transmitter modulates its data with a unique, mutually orthogonal pseudo-random or Walsh spreading code. The receiver uses cross-correlation to separate signals without mutual interference.

---

### Q2: What is a Walsh Code and how is it generated?
- A Walsh code is an orthogonal bipolar sequence (composed of $+1$ and $-1$) derived from a square **Hadamard Matrix** $W_N$ of order $N$ (where $N = 2^k$ is a power of 2).
- It is generated recursively via **Sylvester's construction**:
  $$W_1 = [1]$$
  $$W_{2k} = \begin{bmatrix} W_k & W_k \\ W_k & -W_k \end{bmatrix}$$
- For order $N=2$:
  $$W_2 = \begin{bmatrix} +1 & +1 \\ +1 & -1 \end{bmatrix}$$
- For order $N=4$:
  $$W_4 = \begin{bmatrix} +1 & +1 & +1 & +1 \\ +1 & -1 & +1 & -1 \\ +1 & +1 & -1 & -1 \\ +1 & -1 & -1 & +1 \end{bmatrix}$$

---

### Q3: What is the mathematical definition of Orthogonality?
- Two distinct discrete vectors $C_i$ and $C_j$ of length $N$ are **orthogonal** if their inner product (dot product) equals zero:
  $$C_i \cdot C_j = \sum_{k=0}^{N-1} C_i[k] \cdot C_j[k] = 0 \quad (\forall i \neq j)$$
- The auto-correlation (inner product with itself) equals the sequence length $N$:
  $$C_i \cdot C_i = \sum_{k=0}^{N-1} (C_i[k])^2 = N$$
- In normalized Kronecker delta form:
  $$\frac{1}{N} (C_i \cdot C_j) = \delta_{ij} = \begin{cases} 1 & \text{if } i = j \\ 0 & \text{if } i \neq j \end{cases}$$

---

### Q4: How is a data bit encoded and transmitted in CDMA?
1. Convert the binary bit to a bipolar value $d_i$:
   - Bit `1` $\to d_i = +1$
   - Bit `0` $\to d_i = -1$
   - Idle / Silent (no data) $\to d_i = 0$
2. Multiply $d_i$ by the station's assigned Walsh code $C_i$:
   $$S_i = d_i \cdot C_i = [d_i C_i[0], d_i C_i[1], \dots, d_i C_i[N-1]]$$
3. Each sub-symbol in the spreading sequence is called a **chip**. A single bit is represented by $N$ chips.

---

### Q5: What happens on the physical channel when multiple stations transmit simultaneously?
- The common physical medium acts as a **linear adder**. The instantaneous electromagnetic waves or voltages sum together point-by-point:
  $$C_{\text{composite}} = \sum_{i=0}^{n-1} S_i = \sum_{i=0}^{n-1} d_i \cdot C_i$$
- For $n$ active stations, the composite signal vector contains values ranging between $-n$ and $+n$.

---

### Q6: How does a receiver decode the transmission from a specific station?
- The receiver computes the dot product of the composite signal $C_{\text{composite}}$ with the target station's Walsh code $C_j$, then divides by $N$:
  $$\text{Normalized Output} = \frac{C_{\text{composite}} \cdot C_j}{N} = \frac{1}{N} \left( \sum_{i=0}^{n-1} d_i C_i \right) \cdot C_j = \frac{1}{N} \sum_{i=0}^{n-1} d_i (C_i \cdot C_j)$$
- Since $C_i \cdot C_j = 0$ for all $i \ne j$, and $C_j \cdot C_j = N$:
  $$\text{Normalized Output} = \frac{1}{N} [d_j \cdot N] = d_j$$
- Decision Rule:
  - If $+1 \to$ Original bit was `1`.
  - If $-1 \to$ Original bit was `0`.
  - If $0 \to$ Station was idle / silent.

---

## 📌 Section 2: Mathematical Walkthrough Example

### Q7: Given 4 stations with $W_4$, suppose:
- Station 0 sends bit `1`
- Station 1 sends bit `0`
- Station 2 sends bit `1`
- Station 3 sends bit `0`

**Calculate the composite channel vector and decode Station 2's bit.**

**Solution**:
1. Walsh codes from $W_4$:
   - $C_0 = [+1, +1, +1, +1]$
   - $C_1 = [+1, -1, +1, -1]$
   - $C_2 = [+1, +1, -1, -1]$
   - $C_3 = [+1, -1, -1, +1]$
2. Bipolar bits:
   - $d_0 = +1$
   - $d_1 = -1$
   - $d_2 = +1$
   - $d_3 = -1$
3. Transmitted chip sequences:
   - $S_0 = +1 \times [+1, +1, +1, +1] = [+1, +1, +1, +1]$
   - $S_1 = -1 \times [+1, -1, +1, -1] = [-1, +1, -1, +1]$
   - $S_2 = +1 \times [+1, +1, -1, -1] = [+1, +1, -1, -1]$
   - $S_3 = -1 \times [+1, -1, -1, +1] = [-1, +1, +1, -1]$
4. Composite channel signal:
   $$C_{\text{composite}} = S_0 + S_1 + S_2 + S_3 = [0, +4, 0, 0]$$
5. Decoding Station 2 at Receiver:
   $$C_{\text{composite}} \cdot C_2 = (0)(+1) + (4)(+1) + (0)(-1) + (0)(-1) = 0 + 4 + 0 + 0 = +4$$
   $$\text{Normalized} = \frac{+4}{N} = \frac{+4}{4} = +1$$
   Since $+1 > 0$, the recovered bit is **`1`**, matching Station 2's transmitted bit perfectly!

---

## 📌 Section 3: Advanced Concepts & Real-World Networking

### Q8: What is a "Chip" and "Chip Rate"?
- A **chip** is the smallest pulse/symbol in a spread-spectrum transmission sequence.
- The **Chip Rate** ($R_c$) is the number of chips transmitted per second (e.g. 1.2288 Mcps in IS-95 CDMA).
- The **Bit Rate** ($R_b$) is the information data rate:
  $$R_b = \frac{R_c}{N}$$
  where $N$ is the spreading factor (number of chips per bit).

---

### Q9: What is Processing Gain ($G_p$)?
- Processing Gain is the ratio of transmission bandwidth ($W_{ss}$) to baseband information bandwidth ($B$), or equivalently the spreading factor $N$:
  $$G_p = \frac{R_c}{R_b} = N$$
  In decibels:
  $$G_p (\text{dB}) = 10 \log_{10}(N)$$
- **Significance**: A higher processing gain means CDMA can successfully extract weak desired signals buried deep underneath ambient thermal noise and interference! For $N=64$, $G_p \approx 18.06\text{ dB}$.

---

### Q10: What is the "Near-Far Problem" in CDMA?
- In wireless environments, if Station A is close to the receiver/base station and Station B is far away, Station A's received power may be orders of magnitude stronger than Station B's ($P_A \gg P_B$).
- If spreading codes are slightly non-orthogonal due to multipath delay or clock jitter, Station A's strong signal will drown out Station B.
- **Solution**: Fast closed-loop **Transmit Power Control (TPC)**, where the base station commands transmitters to adjust output power thousands of times per second so all signals arrive at equal power.

---

### Q11: What is the difference between Walsh Codes and PN (Pseudo-Noise) / m-sequences / Gold codes?
- **Walsh Codes**: Strictly orthogonal when **perfectly synchronized** ($\Delta t = 0$). However, their cross-correlation is poor under non-zero time shifts ($\tau \ne 0$). Hence used for **synchronous downlink** (base station to mobile) in CDMA.
- **Gold Codes / Kasami Codes**: Good auto-correlation and bounded low cross-correlation for **arbitrary asynchronous time shifts**. Used for **asynchronous uplink** (mobile to base station) and in GPS.

---

### Q12: How does our assignment implement Data Link Layer integrity?
- Adheres to the requirement: *"Use the same sender-receiver design as previous assignments"*.
- Every station builds an **IEEE 802 standard frame** containing:
  - 48-bit Source and Destination MAC addresses
  - 16-bit payload length
  - 8-bit sequence number
  - Variable-length user payload
  - **32-bit CRC (FCS)** using IEEE 802.3 standard generator polynomial
- Spreads the frame bitstream into Walsh chips, transmits over the common channel, despreads at receivers, and recalculates CRC-32. A 100% CRC pass proves zero frame corruption.
