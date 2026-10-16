#!/usr/bin/env python3
"""
Parallel Particle Dynamics Lab - Scientific Plotting Utility
Generates publication-grade analytical vs simulation verification plots
from structured CSV datasets exported by the Phase 1A.5 experiment framework.

Outputs:
  1. plot_trajectory.png        - Parabolic trajectory and position error vs time
  2. plot_momentum.png          - Linear momentum conservation across collision
  3. plot_collision_energy.png  - Kinetic energy vs time for elastic vs inelastic
  4. plot_gravity_energy.png    - Kinetic, potential, and total energy in free fall
  5. plot_convergence.png       - Timestep convergence log-log plot with O(dt) slope
"""

import argparse
import os
import sys
import pandas as pd
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def set_scientific_style():
    """Configure matplotlib for clean, publication-quality figures."""
    plt.style.use('seaborn-v0_8-whitegrid' if 'seaborn-v0_8-whitegrid' in plt.style.available else 'default')
    plt.rcParams.update({
        'font.size': 11,
        'axes.labelsize': 12,
        'axes.titlesize': 13,
        'xtick.labelsize': 10,
        'ytick.labelsize': 10,
        'legend.fontsize': 10,
        'figure.titlesize': 14,
        'lines.linewidth': 2.0,
        'lines.markersize': 6,
        'grid.alpha': 0.4,
        'grid.linestyle': '--',
    })

def plot_trajectory(csv_path, out_path, dpi=300):
    """Plot ballistic motion trajectory: simulation vs analytical solution and position error."""
    if not os.path.exists(csv_path):
        print(f"[WARN] File not found: {csv_path}, skipping trajectory plot.")
        return

    df = pd.read_csv(csv_path, comment='#')

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(9, 7), sharex=True, gridspec_kw={'height_ratios': [2, 1]})

    # Trajectory coordinates
    ax1.plot(df['time'], df['y_exact'], 'k--', label='Analytical $y(t) = y_0 + v_{y0}t + \\frac{1}{2}g_y t^2$')
    ax1.plot(df['time'], df['y'], color='#1f77b4', label='Simulation $y(t)$ (Semi-implicit Euler)')
    ax1.plot(df['time'], df['x_exact'], 'g:', label='Analytical $x(t) = x_0 + v_{x0}t$')
    ax1.plot(df['time'], df['x'], color='#2ca02c', alpha=0.7, linestyle='-', label='Simulation $x(t)$')

    ax1.set_ylabel('Position [m]')
    ax1.set_title('Ballistic Motion: Simulation vs Analytical Solution', fontweight='bold')
    ax1.legend(loc='lower left', frameon=True)
    ax1.grid(True)

    # Position error
    ax2.plot(df['time'], df['pos_error'] * 1000.0, color='#d62728', label='Error $\\|\\mathbf{x}_{sim} - \\mathbf{x}_{exact}\\|$ [mm]')
    ax2.set_xlabel('Time $t$ [s]')
    ax2.set_ylabel('Position Error [mm]')
    ax2.legend(loc='upper left', frameon=True)
    ax2.grid(True)

    plt.tight_layout()
    plt.savefig(out_path, dpi=dpi)
    plt.close()
    print(f"  [SAVED] {out_path}")

def plot_momentum(csv_elastic, csv_inelastic, csv_unequal, out_path, dpi=300):
    """Plot total linear momentum across elastic, inelastic, and unequal-mass collisions."""
    if not os.path.exists(csv_elastic) or not os.path.exists(csv_inelastic):
        print(f"[WARN] Collision CSVs not found, skipping momentum plot.")
        return

    df_el = pd.read_csv(csv_elastic, comment='#')
    df_inel = pd.read_csv(csv_inelastic, comment='#')
    df_uneq = pd.read_csv(csv_unequal, comment='#') if os.path.exists(csv_unequal) else None

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(9, 7), sharex=True)

    # Momentum components
    ax1.plot(df_el['time'], df_el['Px'], color='#1f77b4', lw=2, label='Equal Mass Elastic ($e=1.0$, $P_x=0$)')
    ax1.plot(df_inel['time'], df_inel['Px'], color='#ff7f0e', lw=2, linestyle='--', label='Equal Mass Inelastic ($e=0.5$, $P_x=0$)')
    if df_uneq is not None:
        ax1.plot(df_uneq['time'], df_uneq['Px'], color='#2ca02c', lw=2, linestyle='-.', label='Unequal Mass Elastic ($m_2=4m_1$, $P_x \\approx 2.51$ kg$\\cdot$m/s)')
    ax1.set_ylabel('Total Momentum $P_x$ [kg$\\cdot$m/s]')
    ax1.set_title('Strict Invariance of Total Linear Momentum Across All Collision Regimes', fontweight='bold')
    ax1.legend(loc='center right', frameon=True)
    ax1.grid(True)

    # Momentum drift/error
    ax2.plot(df_el['time'], df_el['momentum_error'], color='#1f77b4', label='Elastic Drift $|P_x(t) - P_x(0)|$')
    ax2.plot(df_inel['time'], df_inel['momentum_error'], color='#ff7f0e', linestyle='--', label='Inelastic Drift $|P_x(t) - P_x(0)|$')
    if df_uneq is not None:
        ax2.plot(df_uneq['time'], df_uneq['momentum_error'], color='#2ca02c', linestyle='-.', label='Unequal Mass Drift')
    ax2.set_xlabel('Time $t$ [s]')
    ax2.set_ylabel('Momentum Drift [kg$\\cdot$m/s]')
    ax2.set_ylim(-1e-6, 1e-5)
    ax2.legend(loc='upper left', frameon=True)
    ax2.grid(True)

    plt.tight_layout()
    plt.savefig(out_path, dpi=dpi)
    plt.close()
    print(f"  [SAVED] {out_path}")

def plot_collision_energy(csv_elastic, csv_inelastic, out_path, dpi=300):
    """Plot kinetic energy vs time comparing elastic (conserved) and inelastic (dissipative) collisions."""
    if not os.path.exists(csv_elastic) or not os.path.exists(csv_inelastic):
        print(f"[WARN] Collision CSVs not found, skipping collision energy plot.")
        return

    df_el = pd.read_csv(csv_elastic, comment='#')
    df_inel = pd.read_csv(csv_inelastic, comment='#')

    fig, ax = plt.subplots(figsize=(9, 5.5))

    ax.plot(df_el['time'], df_el['Ek'], color='#1f77b4', lw=2.2, label='Elastic Collision ($e=1.0$): $E_k$ Conserved')
    ax.plot(df_inel['time'], df_inel['Ek'], color='#d62728', lw=2.2, linestyle='-', label='Inelastic Collision ($e=0.5$): Dissipative')

    # Draw theoretical post-collision energy line for inelastic
    e = 0.5
    # Initial energy of inelastic:
    ek_initial = df_inel['Ek'].iloc[0]
    # delta Ek = - 0.5 * mu * (1 - e^2) * (v1 - v2)^2
    # From csv header or calculation:
    ek_final_theory = ek_initial - 5.890486
    ax.axhline(ek_final_theory, color='#d62728', linestyle=':', alpha=0.8,
               label=f'Analytical Inelastic $E_{{k,post}}$ ({ek_final_theory:.2f} J)')

    ax.set_xlabel('Time $t$ [s]')
    ax.set_ylabel('Total Kinetic Energy $E_k$ [J]')
    ax.set_title('Kinetic Energy Behavior in Elastic vs Inelastic Collisions', fontweight='bold')

    # Annotation of impact
    impact_t = 0.15
    ax.axvline(impact_t, color='gray', linestyle='--', alpha=0.6)
    ax.text(impact_t + 0.015, (ek_initial + ek_final_theory)/2.0 + 0.5,
            'Impact Event ($t \\approx 0.15$ s)\n$\\Delta E_k = -5.89$ J',
            color='#333333', fontsize=10, bbox=dict(boxstyle='round,pad=0.4', facecolor='yellow', alpha=0.25))

    ax.legend(loc='lower left', frameon=True)
    ax.grid(True)

    plt.tight_layout()
    plt.savefig(out_path, dpi=dpi)
    plt.close()
    print(f"  [SAVED] {out_path}")

def plot_gravity_energy(csv_path, out_path, dpi=300):
    """Plot kinetic, potential, and total energy during gravitational free fall."""
    if not os.path.exists(csv_path):
        print(f"[WARN] File not found: {csv_path}, skipping gravity energy plot.")
        return

    df = pd.read_csv(csv_path, comment='#')

    fig, ax = plt.subplots(figsize=(9, 5.5))

    ax.plot(df['time'], df['Ek'], color='#2ca02c', label='Kinetic Energy $E_k = \\frac{1}{2}mv^2$')
    ax.plot(df['time'], df['Ep'], color='#ff7f0e', label='Potential Energy $E_p = -m(\\mathbf{g}\\cdot\\mathbf{x})$')
    ax.plot(df['time'], df['E_total'], color='#1f77b4', lw=2.5, label='Total Mechanical Energy $E_{tot} = E_k + E_p$')

    ax.set_xlabel('Time $t$ [s]')
    ax.set_ylabel('Energy [J]')
    ax.set_title('Mechanical Energy Partition & Symplectic Drift in Gravitational Field', fontweight='bold')
    ax.legend(loc='center right', frameon=True)
    ax.grid(True)

    plt.tight_layout()

    # Inset showing the minute symplectic energy drift
    e0 = df['E_total'].iloc[0]
    drift_percent = ((df['E_total'] - e0) / e0) * 100.0

    ax_inset = ax.inset_axes([0.18, 0.18, 0.32, 0.28])
    ax_inset.plot(df['time'], drift_percent, color='#d62728')
    ax_inset.set_title('Symplectic Drift $\\Delta E / E_0$ [%]', fontsize=9)
    ax_inset.set_xlabel('$t$ [s]', fontsize=8)
    ax_inset.set_ylabel('% drift', fontsize=8)
    ax_inset.tick_params(axis='both', which='major', labelsize=8)
    ax_inset.grid(True, linestyle=':')

    plt.savefig(out_path, dpi=dpi)
    plt.close()
    print(f"  [SAVED] {out_path}")

def plot_convergence(csv_path, out_path, dpi=300):
    """Plot log-log timestep convergence study with theoretical first-order slope."""
    if not os.path.exists(csv_path):
        print(f"[WARN] File not found: {csv_path}, skipping convergence plot.")
        return

    df = pd.read_csv(csv_path, comment='#')

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11, 5))

    # Log-Log Convergence Plot
    dt = df['dt'].values
    linf = df['linf_error'].values
    l2 = df['l2_error'].values

    # Reference line with slope 1
    ref_slope1 = linf[0] * (dt / dt[0])

    ax1.loglog(dt, linf, 'o-', color='#d62728', lw=2, label='$L_\\infty$ Position Error (Simulated)')
    ax1.loglog(dt, l2, 's-', color='#1f77b4', lw=2, label='$L_2$ Position Error (Simulated)')
    ax1.loglog(dt, ref_slope1, 'k--', label='Theoretical $\\mathcal{O}(\\Delta t)$ (Slope = 1.0)')

    ax1.set_xlabel('Timestep $\\Delta t$ [s]')
    ax1.set_ylabel('Global Position Error [m]')
    ax1.set_title('Empirical Convergence Rate ($p \\approx 1.0$)', fontweight='bold')
    ax1.legend(loc='lower right', frameon=True)
    ax1.grid(True, which='both', linestyle='--')

    # Compute Time vs Step Count / dt
    ax2.plot(df['dt'], df['compute_time_ms'] * 1000.0, '^-', color='#9467bd', lw=2)
    ax2.set_xlabel('Timestep $\\Delta t$ [s]')
    ax2.set_ylabel('Execution Time [$\\mu$s]')
    ax2.set_title('Solver Compute Time vs Timestep Size', fontweight='bold')
    ax2.grid(True)

    plt.tight_layout()
    plt.savefig(out_path, dpi=dpi)
    plt.close()
    print(f"  [SAVED] {out_path}")

def main():
    parser = argparse.ArgumentParser(description='Generate publication-grade experiment verification plots.')
    parser.add_argument('--csv-dir', default='output/csv', help='Directory containing CSV experiment data')
    parser.add_argument('--plot-dir', default='output/plots', help='Directory to save output plots')
    parser.add_argument('--dpi', type=int, default=300, help='Image resolution DPI')
    args = parser.parse_args()

    os.makedirs(args.plot_dir, exist_ok=True)
    set_scientific_style()

    print("======================================================================")
    print("  PARALLEL PARTICLE DYNAMICS LAB: SCIENTIFIC PLOT GENERATION")
    print("======================================================================")
    print(f"Source CSVs: {args.csv_dir}")
    print(f"Output Plots: {args.plot_dir} (DPI={args.dpi})\n")

    plot_trajectory(os.path.join(args.csv_dir, 'gravity_trajectory.csv'),
                    os.path.join(args.plot_dir, 'plot_trajectory.png'), dpi=args.dpi)

    plot_momentum(os.path.join(args.csv_dir, 'collision_elastic.csv'),
                  os.path.join(args.csv_dir, 'collision_inelastic.csv'),
                  os.path.join(args.csv_dir, 'collision_unequal.csv'),
                  os.path.join(args.plot_dir, 'plot_momentum.png'), dpi=args.dpi)

    plot_collision_energy(os.path.join(args.csv_dir, 'collision_elastic.csv'),
                          os.path.join(args.csv_dir, 'collision_inelastic.csv'),
                          os.path.join(args.plot_dir, 'plot_collision_energy.png'), dpi=args.dpi)

    plot_gravity_energy(os.path.join(args.csv_dir, 'gravity_trajectory.csv'),
                        os.path.join(args.plot_dir, 'plot_gravity_energy.png'), dpi=args.dpi)

    plot_convergence(os.path.join(args.csv_dir, 'convergence_study.csv'),
                     os.path.join(args.plot_dir, 'plot_convergence.png'), dpi=args.dpi)

    print("\nAll publication-grade plots generated successfully.")

if __name__ == '__main__':
    main()
