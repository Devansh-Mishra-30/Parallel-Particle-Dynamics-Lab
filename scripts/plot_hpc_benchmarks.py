#!/usr/bin/env python3
"""
Parallel Particle Dynamics Lab - HPC Benchmark Plotting Utility
Generates publication-grade performance, scaling, and architectural breakdown plots
from structured CSV datasets exported by the Phase 2 (OpenMP) and Phase 3 (CUDA) harnesses.

Outputs:
  1. output/plots/plot_openmp_scaling.png        - OpenMP speedup vs thread count with Amdahl fit
  2. output/plots/plot_broadphase_breakdown.png   - Algorithmic broadphase vs hardware multicore acceleration
  3. output/plots/plot_cross_backend_scaling.png - Cross-backend scaling (CPU, OpenMP, CUDA) and throughput
"""

import os
import sys
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def set_scientific_style():
    """Configure matplotlib for publication-quality figures."""
    plt.style.use('seaborn-v0_8-whitegrid' if 'seaborn-v0_8-whitegrid' in plt.style.available else 'default')
    plt.rcParams.update({
        'font.size': 11,
        'axes.labelsize': 12,
        'axes.titlesize': 13,
        'xtick.labelsize': 10,
        'ytick.labelsize': 10,
        'legend.fontsize': 10,
        'figure.titlesize': 14,
        'lines.linewidth': 2.2,
        'lines.markersize': 7,
        'grid.alpha': 0.45,
        'grid.linestyle': '--',
    })

def plot_openmp_scaling(csv_path, out_path, dpi=300):
    """Plot OpenMP speedup S(T) vs thread count T with Amdahl's Law model curve."""
    if not os.path.exists(csv_path):
        print(f"[WARN] File not found: {csv_path}, skipping OpenMP scaling plot.")
        return

    df = pd.read_csv(csv_path, comment='#')

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5.5))

    # Subset particle counts
    particles_list = [256, 512, 1024]
    palette = {'256': '#2ca02c', '512': '#ff7f0e', '1024': '#1f77b4'}

    # 1. Speedup S(T)
    thread_domain = np.linspace(1, 16, 100)
    # Ideal linear
    ax1.plot(thread_domain, thread_domain, 'k--', alpha=0.5, label='Ideal Linear Speedup ($S=T$)')

    # Theoretical Amdahl fit for f = 0.85
    f_amdahl = 0.85
    s_amdahl = 1.0 / ((1.0 - f_amdahl) + (f_amdahl / thread_domain))
    ax1.plot(thread_domain, s_amdahl, 'r-.', linewidth=1.8, label=r"Amdahl's Law ($f=85\%$)")

    for p in particles_list:
        sub = df[(df['particles'] == p) & (df['threads'] > 0)].sort_values('threads')
        if not sub.empty:
            ax1.plot(sub['threads'], sub['speedup'], marker='o', label=f'Measured $N={p}$', color=palette[str(p)])

    ax1.set_xlabel('Thread Count $T$')
    ax1.set_ylabel('Speedup $S(T) = t_1 / t_T$')
    ax1.set_title('OpenMP Strong Scaling Speedup', fontweight='bold')
    ax1.set_xticks([1, 2, 4, 8, 16])
    ax1.set_ylim(0, 8.5)
    ax1.legend(loc='upper left', frameon=True)
    ax1.grid(True)

    # 2. Parallel Efficiency E(T) = S(T) / T
    ax2.plot(thread_domain, np.ones_like(thread_domain) * 100.0, 'k--', alpha=0.5, label='100% Ideal Efficiency')
    for p in particles_list:
        sub = df[(df['particles'] == p) & (df['threads'] > 0)].sort_values('threads')
        if not sub.empty:
            eff_pct = sub['efficiency'] * 100.0
            ax2.plot(sub['threads'], eff_pct, marker='s', label=f'$N={p}$', color=palette[str(p)])

    ax2.set_xlabel('Thread Count $T$')
    ax2.set_ylabel('Parallel Efficiency $E(T)$ [%]')
    ax2.set_title('Parallel Efficiency vs Thread Count', fontweight='bold')
    ax2.set_xticks([1, 2, 4, 8, 16])
    ax2.set_ylim(0, 110)
    ax2.legend(loc='upper right', frameon=True)
    ax2.grid(True)

    plt.tight_layout()
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    plt.savefig(out_path, dpi=dpi)
    plt.close()
    print(f"[INFO] Saved OpenMP scaling plot to {out_path}")

def plot_broadphase_breakdown(csv_path, out_path, dpi=300):
    """Plot algorithmic acceleration (Spatial Grid) vs multicore OpenMP parallelism."""
    if not os.path.exists(csv_path):
        print(f"[WARN] File not found: {csv_path}, skipping broadphase breakdown plot.")
        return

    df = pd.read_csv(csv_path, comment='#')

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5.5))

    # 1. Execution times on Log Scale
    ax1.plot(df['particles'], df['mode_A_seq_allpairs_ms'], marker='o', color='#d62728', label='Mode A: CPU All-Pairs $O(N^2)$')
    ax1.plot(df['particles'], df['mode_C_omp_allpairs_ms'], marker='^', color='#ff7f0e', label='Mode C: OpenMP 8T All-Pairs $O(N^2)$')
    ax1.plot(df['particles'], df['mode_B_seq_grid_ms'], marker='s', color='#2ca02c', label='Mode B: CPU Spatial Grid $O(N)$')
    ax1.plot(df['particles'], df['mode_D_omp_grid_ms'], marker='D', color='#1f77b4', label='Mode D: OpenMP 8T Spatial Grid $O(N)$')

    ax1.set_xscale('log', base=2)
    ax1.set_yscale('log')
    ax1.set_xlabel('Particle Count $N$')
    ax1.set_ylabel('Step Compute Time [ms] (Log Scale)')
    ax1.set_title('Algorithmic vs Multicore Acceleration', fontweight='bold')
    ax1.set_xticks(df['particles'])
    ax1.set_xticklabels([str(p) for p in df['particles']])
    ax1.legend(loc='upper left', frameon=True)
    ax1.grid(True, which="both", ls="--")

    # 2. Speedup comparison at N=1024 (Grouped Bars)
    sub1024 = df[df['particles'] == 1024].iloc[0]
    modes = ['Mode B: Algo Only\n(Seq Grid)', 'Mode C: Parallel Only\n(OpenMP 8T All-Pairs)', 'Mode D: Combined\n(OpenMP 8T Grid)']
    speedups = [sub1024['speedup_B_algo'], sub1024['speedup_C_omp'], sub1024['speedup_D_combined']]
    colors = ['#2ca02c', '#ff7f0e', '#1f77b4']

    bars = ax2.bar(modes, speedups, color=colors, width=0.55, edgecolor='black', linewidth=1.2)
    for bar in bars:
        h = bar.get_height()
        ax2.text(bar.get_x() + bar.get_width()/2., h + 0.6, f'{h:.2f}x', ha='center', va='bottom', fontweight='bold', fontsize=11)

    ax2.set_ylabel('Speedup Relative to Serial All-Pairs')
    ax2.set_title('Speedup Disentanglement at $N = 1,024$', fontweight='bold')
    ax2.set_ylim(0, 40)
    ax2.grid(True, axis='y')

    plt.tight_layout()
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    plt.savefig(out_path, dpi=dpi)
    plt.close()
    print(f"[INFO] Saved broadphase breakdown plot to {out_path}")

def plot_cross_backend_scaling(csv_path, out_path, dpi=300):
    """Plot cross-backend scaling (CPU, OpenMP, CUDA) and throughput across N=100..10,000."""
    if not os.path.exists(csv_path):
        print(f"[WARN] File not found: {csv_path}, skipping cross-backend scaling plot.")
        return

    df = pd.read_csv(csv_path, comment='#')

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8.5), sharex=True)

    backends = {
        'CPU_Serial': ('Sequential CPU (Spatial Grid)', '#2ca02c', 'o', '-'),
        'OpenMP_8T': ('OpenMP Multicore 8T (Spatial Grid)', '#ff7f0e', 's', '-'),
        'CUDA_RTX4060': ('NVIDIA CUDA GPU (RTX 4060 Laptop)', '#1f77b4', 'D', '-')
    }

    # 1. Step compute time (ms) vs N
    for b_key, (b_label, color, marker, ls) in backends.items():
        sub = df[df['backend'] == b_key].sort_values('particles')
        if not sub.empty:
            ax1.plot(sub['particles'], sub['median_ms'], marker=marker, color=color, linestyle=ls, label=b_label)

    # Annotate Latency Floor & Crossover
    ax1.axhline(y=1.4, color='gray', linestyle=':', alpha=0.7, label='GPU Overhead Floor (~1.4 ms)')
    ax1.axvline(x=1000, color='purple', linestyle='--', alpha=0.6, label='CPU/GPU Crossover ($N \\approx 1,000$)')

    ax1.set_xscale('log')
    ax1.set_yscale('log')
    ax1.set_ylabel('Step Compute Time [ms] (Log Scale)')
    ax1.set_title('Cross-Backend Scaling: Execution Latency vs Particle Count', fontweight='bold')
    ax1.legend(loc='upper left', frameon=True)
    ax1.grid(True, which="both", ls="--")

    # 2. Throughput (Particles / Second) vs N
    for b_key, (b_label, color, marker, ls) in backends.items():
        sub = df[df['backend'] == b_key].sort_values('particles')
        if not sub.empty:
            throughput_m = sub['throughput_p_per_sec'] / 1e6
            ax2.plot(sub['particles'], throughput_m, marker=marker, color=color, linestyle=ls, label=b_label)

    ax2.set_xscale('log')
    ax2.set_xlabel('Particle Count $N$')
    ax2.set_ylabel('Throughput [Million Particles / sec]')
    ax2.set_title('Computational Throughput Scaling', fontweight='bold')
    ax2.set_xticks([100, 500, 1000, 5000, 10000])
    ax2.set_xticklabels(['100', '500', '1k', '5k', '10k'])
    ax2.legend(loc='upper left', frameon=True)
    ax2.grid(True, which="both", ls="--")

    plt.tight_layout()
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    plt.savefig(out_path, dpi=dpi)
    plt.close()
    print(f"[INFO] Saved cross-backend scaling plot to {out_path}")

def main():
    set_scientific_style()
    benchmark_dir = "output/benchmarks"
    plot_dir = "output/plots"

    plot_openmp_scaling(
        os.path.join(benchmark_dir, "openmp_scaling_results.csv"),
        os.path.join(plot_dir, "plot_openmp_scaling.png")
    )

    plot_broadphase_breakdown(
        os.path.join(benchmark_dir, "broadphase_and_omp_breakdown.csv"),
        os.path.join(plot_dir, "plot_broadphase_breakdown.png")
    )

    plot_cross_backend_scaling(
        os.path.join(benchmark_dir, "cross_backend_scaling_results.csv"),
        os.path.join(plot_dir, "plot_cross_backend_scaling.png")
    )

if __name__ == '__main__':
    main()
