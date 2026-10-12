"""
============================================================================
Assignment 4: CDMA with Walsh Code - Simulation & Performance Analysis
CSE/PC/B/S/314: Computer Networks Lab

Description:
  Comprehensive Python analysis suite for CDMA Spread Spectrum evaluation:
  1. Walsh code orthogonality verification and cross-correlation heatmap.
  2. Multi-station chip waveform generation and channel superposition plots.
  3. Bit Error Rate (BER) vs SNR waterfall curves showing CDMA processing gain.
  4. Aggregate throughput and capacity scaling as number of stations increases.
  5. Export of empirical simulation results to results.csv.
============================================================================
"""

import os
import math
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

# Set aesthetic plot styling
plt.style.use('seaborn-v0_8-whitegrid' if 'seaborn-v0_8-whitegrid' in plt.style.available else 'default')
plt.rcParams['font.sans-serif'] = 'Arial'
plt.rcParams['font.family'] = 'sans-serif'
plt.rcParams['figure.dpi'] = 300

def generate_walsh_matrix(order):
    """Recursively generates Hadamard-Walsh matrix of order N (power of 2)."""
    p = 1
    while p < order:
        p <<= 1
    order = p

    h = np.array([[1]], dtype=int)
    while h.shape[0] < order:
        h = np.block([[h, h], [h, -h]])
    return h

def plot_orthogonality_matrix(output_path="cdma_orthogonality_matrix.png", N=8):
    """Plots the cross-correlation matrix demonstrating absolute zero cross-talk."""
    W = generate_walsh_matrix(N)
    # Compute normalized correlation matrix: (W @ W.T) / N
    corr = (W @ W.T) / N

    fig, ax = plt.subplots(figsize=(8, 7))
    cax = ax.matshow(corr, cmap="coolwarm", vmin=-1.0, vmax=1.0)
    fig.colorbar(cax, ax=ax, fraction=0.046, pad=0.04, label="Normalized Cross-Correlation")

    for i in range(N):
        for j in range(N):
            val = corr[i, j]
            color = "white" if abs(val) > 0.5 else "black"
            ax.text(j, i, f"{val:.1f}", ha="center", va="center", color=color, fontsize=11, fontweight="bold")

    ax.set_xticks(range(N))
    ax.set_yticks(range(N))
    ax.set_xticklabels([f"C_{i}" for i in range(N)], fontsize=11)
    ax.set_yticklabels([f"C_{i}" for i in range(N)], fontsize=11)
    ax.set_title(f"Hadamard-Walsh Orthogonality & Cross-Correlation Matrix (N={N})\n"
                 r"$C_i \cdot C_j = N \cdot \delta_{ij}$ (Zero Inter-Station Cross-Talk)",
                 fontsize=13, fontweight="bold", pad=20)
    plt.tight_layout()
    plt.savefig(output_path, dpi=300)
    plt.close()
    print(f"[PLOT] Saved {output_path}")

def plot_signal_waveforms(output_path="cdma_signal_waveforms.png"):
    """Plots individual station data bits, chip sequences, and composite channel waveform."""
    N = 4
    W = generate_walsh_matrix(N)
    
    # 4 stations with 4 data bits each
    # Bits: Station 0: [1, 0, 1, 1], Station 1: [0, 1, 1, 0], Station 2: [1, 1, 0, 1], Station 3: [0, 0, 1, 0]
    data_bits = [
        np.array([1, 0, 1, 1]),
        np.array([0, 1, 1, 0]),
        np.array([1, 1, 0, 1]),
        np.array([0, 0, 1, 0])
    ]
    bipolar_bits = [2 * b - 1 for b in data_bits] # 1 -> +1, 0 -> -1
    
    # Spread each bit by N chips
    station_chips = []
    for s in range(4):
        chips = []
        for bit in bipolar_bits[s]:
            chips.extend(bit * W[s])
        station_chips.append(np.array(chips))
    
    # Composite channel signal
    composite = sum(station_chips)
    
    # Despread at Receiver 0 using C_0
    recovered_bits_0 = []
    for b in range(4):
        block = composite[b*N : (b+1)*N]
        dot = np.dot(block, W[0])
        recovered_bits_0.append(1 if dot > 0 else 0)
    
    # Create 6-panel waveform plot
    fig, axes = plt.subplots(6, 1, figsize=(12, 10), sharex=True)
    chip_indices = np.arange(len(composite))
    
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728']
    for s in range(4):
        axes[s].step(chip_indices, station_chips[s], where='mid', color=colors[s], linewidth=1.8)
        axes[s].set_ylabel(f"Station {s}\n(C_{s})", fontsize=10, fontweight="bold")
        axes[s].set_ylim(-1.5, 1.5)
        axes[s].axhline(0, color='gray', linestyle='--', linewidth=0.7)
    
    axes[4].step(chip_indices, composite, where='mid', color='#9467bd', linewidth=2.0)
    axes[4].set_ylabel("Composite\nMedium", fontsize=10, fontweight="bold", color='#9467bd')
    axes[4].set_ylim(-4.5, 4.5)
    axes[4].axhline(0, color='black', linestyle='--', linewidth=0.8)
    
    # Receiver 0 Despreading result
    rec_signal = np.repeat(recovered_bits_0, N)
    axes[5].step(chip_indices, rec_signal, where='mid', color='#1f77b4', linewidth=2.2, linestyle='-')
    axes[5].set_ylabel("Rx 0 Decoded\nPayload", fontsize=10, fontweight="bold")
    axes[5].set_ylim(-0.5, 1.5)
    axes[5].set_xlabel("Chip Time Slot (t)", fontsize=11, fontweight="bold")
    
    # Mark bit boundaries
    for ax in axes:
        for b in range(1, 4):
            ax.axvline(b * N - 0.5, color='gray', linestyle=':', alpha=0.6)
            
    fig.suptitle("CDMA Time-Domain Signal Waveforms & Superposition\n"
                 "(Individual Station Chips, Composite Channel Adder, and Despread Output)",
                 fontsize=14, fontweight="bold")
    plt.tight_layout()
    plt.savefig(output_path, dpi=300)
    plt.close()
    print(f"[PLOT] Saved {output_path}")

def run_ber_vs_snr_simulation(output_plot="cdma_ber_vs_snr.png", csv_output="results.csv"):
    """Simulates BER vs SNR across different Walsh orders N, demonstrating processing gain."""
    snr_range_dB = np.linspace(-6, 14, 11)
    walsh_orders = [4, 8, 16, 32, 64]
    num_bits = 20000

    results_data = []

    plt.figure(figsize=(9, 6))

    for N in walsh_orders:
        W = generate_walsh_matrix(N)
        num_stations = N # Fully loaded cell
        code_word = W[0] # Test Station 0
        
        ber_list = []
        for snr_db in snr_range_dB:
            # Generate random bits for all stations
            bits_all = np.random.choice([-1, 1], size=(num_stations, num_bits))
            
            # Spread: (num_stations, num_bits * N)
            spread_all = np.zeros((num_stations, num_bits * N))
            for s in range(num_stations):
                spread_all[s] = np.kron(bits_all[s], W[s])
                
            # Composite channel signal
            composite = np.sum(spread_all, axis=0)
            
            # Add AWGN
            snr_linear = 10.0 ** (snr_db / 10.0)
            signal_power = np.mean(composite ** 2)
            noise_power = signal_power / snr_linear
            noise = np.random.normal(0, np.sqrt(noise_power), size=composite.shape)
            noisy_composite = composite + noise
            
            # Despread Station 0
            # Reshape into blocks of N chips: (num_bits, N)
            blocks = noisy_composite.reshape((num_bits, N))
            dot_products = np.dot(blocks, code_word)
            decoded_bits = np.where(dot_products >= 0, 1, -1)
            
            # Compute BER
            bit_errors = np.sum(decoded_bits != bits_all[0])
            ber = bit_errors / num_bits
            ber_list.append(ber)
            
            processing_gain = 10.0 * np.log10(N)
            results_data.append({
                "Walsh_Order_N": N,
                "Stations": num_stations,
                "SNR_dB": round(snr_db, 2),
                "Processing_Gain_dB": round(processing_gain, 2),
                "Total_Bits": num_bits,
                "Bit_Errors": int(bit_errors),
                "BER": ber
            })

        # Plot curve
        # Add small floor for log scale if ber == 0
        plot_ber = [max(b, 1e-5) for b in ber_list]
        proc_gain = 10.0 * np.log10(N)
        plt.semilogy(snr_range_dB, plot_ber, marker='o', linewidth=2.0, 
                     label=f"N={N} ({N} stations, Gain={proc_gain:.1f} dB)")

    plt.title("CDMA Bit Error Rate (BER) vs SNR (dB) under AWGN Noise\n"
              r"Demonstrating Processing Gain $G_p = 10\log_{10}(N)$ dB",
              fontsize=13, fontweight="bold")
    plt.xlabel("Signal-to-Noise Ratio (SNR in dB)", fontsize=11, fontweight="bold")
    plt.ylabel("Bit Error Rate (BER)", fontsize=11, fontweight="bold")
    plt.ylim(5e-5, 1.0)
    plt.grid(True, which="both", ls="--", alpha=0.6)
    plt.legend(fontsize=10, loc="lower left")
    plt.tight_layout()
    plt.savefig(output_plot, dpi=300)
    plt.close()
    print(f"[PLOT] Saved {output_plot}")

    # Export to CSV
    df = pd.DataFrame(results_data)
    df.to_csv(csv_output, index=False)
    print(f"[CSV] Saved {csv_output} ({len(df)} rows)")

def plot_throughput_scaling(output_path="cdma_throughput_scaling.png"):
    """Plots aggregate channel throughput and capacity scaling vs number of stations."""
    stations = np.array([1, 2, 4, 8, 16, 32, 64])
    chip_rate_mcps = 1.2288 # Standard cdmaOne / IS-95 chip rate (1.2288 Mcps)
    
    # In CDMA with Walsh code of length N=64:
    # Individual station bit rate = Chip Rate / N
    N = 64
    user_bit_rate_kbps = (chip_rate_mcps * 1e3) / N # 19.2 kbps
    aggregate_throughput = stations * user_bit_rate_kbps

    fig, ax1 = plt.subplots(figsize=(8, 5.5))

    color = '#1f77b4'
    ax1.set_xlabel("Number of Simultaneous Active Stations (n)", fontsize=11, fontweight="bold")
    ax1.set_ylabel("Aggregate Channel Throughput (kbps)", color=color, fontsize=11, fontweight="bold")
    line1 = ax1.plot(stations, aggregate_throughput, color=color, marker='s', linewidth=2.2, label="Aggregate Throughput (kbps)")
    ax1.tick_params(axis='y', labelcolor=color)

    ax2 = ax1.twinx()
    color = '#2ca02c'
    per_user_rate = np.full_like(stations, user_bit_rate_kbps, dtype=float)
    ax2.set_ylabel("Per-Station Dedicated Bit Rate (kbps)", color=color, fontsize=11, fontweight="bold")
    line2 = ax2.plot(stations, per_user_rate, color=color, linestyle='--', marker='^', linewidth=2.0, label="Per-Station Rate (19.2 kbps)")
    ax2.tick_params(axis='y', labelcolor=color)
    ax2.set_ylim(0, 30)

    lines = line1 + line2
    labels = [l.get_label() for l in lines]
    ax1.legend(lines, labels, loc="center right", fontsize=10)

    plt.title("CDMA Linear Capacity Scaling (Walsh Order N=64, Chip Rate=1.2288 Mcps)\n"
              "Simultaneous Transmission Without Collisions or Contention Drops",
              fontsize=12, fontweight="bold")
    plt.tight_layout()
    plt.savefig(output_path, dpi=300)
    plt.close()
    print(f"[PLOT] Saved {output_path}")

if __name__ == "__main__":
    print("===================================================================")
    print("      RUNNING CDMA STATISTICAL SIMULATION & ANALYSIS SUITE         ")
    print("===================================================================")
    plot_orthogonality_matrix("cdma_orthogonality_matrix.png", N=8)
    plot_signal_waveforms("cdma_signal_waveforms.png")
    plot_throughput_scaling("cdma_throughput_scaling.png")
    run_ber_vs_snr_simulation("cdma_ber_vs_snr.png", "results.csv")
    print("===================================================================")
    print("              ALL SIMULATION ARTIFACTS GENERATED SUCCESSFULLY      ")
    print("===================================================================")
