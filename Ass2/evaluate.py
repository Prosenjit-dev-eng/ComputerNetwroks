import os
import csv
import subprocess
import matplotlib.pyplot as plt
import numpy as np

def run_benchmark():
    # Check if evaluation_results.csv exists or run benchmark.exe
    if not os.path.exists("evaluation_results.csv"):
        print("Compiling and running benchmark.cpp...")
        subprocess.run(["g++", "-O3", "-std=c++17", "benchmark.cpp", "frame.cpp", "crc.cpp", "checkSum.cpp", "errorInjection.cpp", "channel.cpp", "-o", "benchmark.exe"], check=True)
        subprocess.run(["benchmark.exe"], check=True)

def parse_results(csv_path="evaluation_results.csv"):
    data = {}
    with open(csv_path, mode='r', newline='') as f:
        reader = csv.DictReader(f)
        for row in reader:
            key = f"{row['Protocol']} (N={row['WindowSize']})"
            if key not in data:
                data[key] = {
                    'error_prob': [],
                    'total_tx': [],
                    'retx': [],
                    'time_ms': [],
                    'throughput_kbps': [],
                    'efficiency': [],
                    'avg_prop': [],
                    'avg_rtt': []
                }
            data[key]['error_prob'].append(float(row['ErrorProb']))
            data[key]['total_tx'].append(int(row['TotalTx']))
            data[key]['retx'].append(int(row['Retransmissions']))
            data[key]['time_ms'].append(float(row['TimeMs']))
            data[key]['throughput_kbps'].append(float(row['ThroughputKbps']))
            data[key]['efficiency'].append(float(row['EfficiencyPercent']))
            data[key]['avg_prop'].append(float(row['AvgPropDelayMs']))
            data[key]['avg_rtt'].append(float(row['AvgRttMs']))
    return data

def plot_all_graphs(data):
    # Set aesthetics
    plt.style.use('seaborn-v0_8-whitegrid' if 'seaborn-v0_8-whitegrid' in plt.style.available else 'default')
    colors = {
        'Stop-and-Wait (N=1)': '#e74c3c',
        'Go-Back-N (N=4)': '#e67e22',
        'Go-Back-N (N=8)': '#f39c12',
        'Selective-Repeat (N=4)': '#27ae60',
        'Selective-Repeat (N=8)': '#2980b9'
    }
    markers = {
        'Stop-and-Wait (N=1)': 'o',
        'Go-Back-N (N=4)': 's',
        'Go-Back-N (N=8)': '^',
        'Selective-Repeat (N=4)': 'D',
        'Selective-Repeat (N=8)': 'v'
    }

    # 1. Efficiency vs Error Probability
    plt.figure(figsize=(10, 6))
    for key, vals in data.items():
        plt.plot(vals['error_prob'], vals['efficiency'], label=key,
                 color=colors.get(key, '#333'), marker=markers.get(key, 'o'), linewidth=2.2, markersize=8)
    plt.title("Flow Control Protocol Efficiency vs. Error / Loss Rate", fontsize=14, fontweight='bold', pad=12)
    plt.xlabel("Error / Loss Probability (0.0 to 0.5)", fontsize=12)
    plt.ylabel("Efficiency (%) = (Original Frames / Total Tx) * 100", fontsize=12)
    plt.ylim(0, 105)
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.legend(fontsize=11, loc='lower left')
    plt.tight_layout()
    plt.savefig("efficiency_vs_error_rate.png", dpi=300)
    plt.close()
    print("Saved 'efficiency_vs_error_rate.png'")

    # 2. Throughput vs Error Probability
    plt.figure(figsize=(10, 6))
    for key, vals in data.items():
        plt.plot(vals['error_prob'], vals['throughput_kbps'], label=key,
                 color=colors.get(key, '#333'), marker=markers.get(key, 'o'), linewidth=2.2, markersize=8)
    plt.title("Effective Throughput vs. Error / Loss Rate", fontsize=14, fontweight='bold', pad=12)
    plt.xlabel("Error / Loss Probability (0.0 to 0.5)", fontsize=12)
    plt.ylabel("Throughput (Kbps)", fontsize=12)
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.legend(fontsize=11, loc='upper right')
    plt.tight_layout()
    plt.savefig("throughput_vs_error_rate.png", dpi=300)
    plt.close()
    print("Saved 'throughput_vs_error_rate.png'")

    # 3. Retransmissions vs Error Probability
    plt.figure(figsize=(10, 6))
    for key, vals in data.items():
        plt.plot(vals['error_prob'], vals['retx'], label=key,
                 color=colors.get(key, '#333'), marker=markers.get(key, 'o'), linewidth=2.2, markersize=8)
    plt.title("Total Retransmissions vs. Error / Loss Rate", fontsize=14, fontweight='bold', pad=12)
    plt.xlabel("Error / Loss Probability (0.0 to 0.5)", fontsize=12)
    plt.ylabel("Number of Retransmissions", fontsize=12)
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.legend(fontsize=11, loc='upper left')
    plt.tight_layout()
    plt.savefig("retransmissions_vs_error_rate.png", dpi=300)
    plt.close()
    print("Saved 'retransmissions_vs_error_rate.png'")

    # 4. Packet Propagation Time vs ACK Reception Time (RTT)
    plt.figure(figsize=(10, 6))
    labels = list(data.keys())
    x = np.arange(len(labels))
    width = 0.35

    # Taking average at p = 0.2
    prop_times = []
    rtt_times = []
    for key in labels:
        # index for error_prob = 0.2
        idx = data[key]['error_prob'].index(0.2) if 0.2 in data[key]['error_prob'] else 2
        prop_times.append(data[key]['avg_prop'][idx])
        rtt_times.append(data[key]['avg_rtt'][idx])

    fig, ax = plt.subplots(figsize=(10, 6))
    rects1 = ax.bar(x - width/2, prop_times, width, label='Packet Propagation Delay (One-Way)', color='#3498db')
    rects2 = ax.bar(x + width/2, rtt_times, width, label='ACK Reception Time (Round-Trip RTT)', color='#9b59b6')

    ax.set_ylabel('Time (ms)', fontsize=12)
    ax.set_title('Packet Propagation Time vs. ACK Reception Time (at Error Rate = 0.2)', fontsize=14, fontweight='bold', pad=12)
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=15, ha='right', fontsize=10)
    ax.legend(fontsize=11)
    ax.grid(True, linestyle='--', alpha=0.6, axis='y')

    for rect in rects1 + rects2:
        height = rect.get_height()
        ax.annotate(f'{height:.1f}ms',
                    xy=(rect.get_x() + rect.get_width() / 2, height),
                    xytext=(0, 3), textcoords="offset points",
                    ha='center', va='bottom', fontsize=9)

    plt.tight_layout()
    plt.savefig("propagation_vs_ack_time.png", dpi=300)
    plt.close()
    print("Saved 'propagation_vs_ack_time.png'")

    # 5. Overall Zero-Error Efficiency vs High-Error Efficiency Bar Chart
    plt.figure(figsize=(10, 6))
    zero_err_eff = [data[k]['efficiency'][0] for k in labels]
    high_err_eff = [data[k]['efficiency'][-1] for k in labels]

    x = np.arange(len(labels))
    width = 0.35

    fig, ax = plt.subplots(figsize=(10, 6))
    r1 = ax.bar(x - width/2, zero_err_eff, width, label='Efficiency at 0% Error (Ideal)', color='#2ecc71')
    r2 = ax.bar(x + width/2, high_err_eff, width, label='Efficiency at 50% Error (Harsh)', color='#e74c3c')

    ax.set_ylabel('Efficiency (%)', fontsize=12)
    ax.set_title('Protocol Efficiency Comparison: Ideal (0% Error) vs Harsh Channel (50% Error)', fontsize=14, fontweight='bold', pad=12)
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=15, ha='right', fontsize=10)
    ax.set_ylim(0, 115)
    ax.legend(fontsize=11)
    ax.grid(True, linestyle='--', alpha=0.6, axis='y')

    for rect in r1 + r2:
        height = rect.get_height()
        ax.annotate(f'{height:.1f}%',
                    xy=(rect.get_x() + rect.get_width() / 2, height),
                    xytext=(0, 3), textcoords="offset points",
                    ha='center', va='bottom', fontsize=9)

    plt.tight_layout()
    plt.savefig("overall_comparison_barchart.png", dpi=300)
    plt.close()
    print("Saved 'overall_comparison_barchart.png'")

if __name__ == "__main__":
    run_benchmark()
    data = parse_results()
    plot_all_graphs(data)
    print("\nAll evaluation graphs generated successfully!")
