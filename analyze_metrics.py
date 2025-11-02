#!/usr/bin/env python3
"""
Analyze and visualize ADIOS2 transfer metrics
Reads transfer_metrics.csv and generates performance plots
"""

import pandas as pd
import matplotlib.pyplot as plt
import sys
import os

def analyze_metrics(csv_file='transfer_metrics.csv'):
    """Analyze transfer metrics and generate plots"""
    
    if not os.path.exists(csv_file):
        print(f"Error: {csv_file} not found!")
        print("Make sure to run the receiver first to generate metrics.")
        return
    
    # Read metrics
    df = pd.read_csv(csv_file)
    
    # Print summary statistics
    print("=" * 60)
    print("ADIOS2 Transfer Performance Summary")
    print("=" * 60)
    print(f"Total steps: {len(df)}")
    print(f"Total data transferred: {df['Size(MB)'].sum():.2f} MB")
    print(f"Total time: {df['Time(s)'].sum():.2f} seconds")
    print(f"\nThroughput Statistics:")
    print(f"  Average: {df['Throughput(MB/s)'].mean():.2f} MB/s ({df['Throughput(Mbps)'].mean():.2f} Mbps)")
    print(f"  Median:  {df['Throughput(MB/s)'].median():.2f} MB/s ({df['Throughput(Mbps)'].median():.2f} Mbps)")
    print(f"  Min:     {df['Throughput(MB/s)'].min():.2f} MB/s ({df['Throughput(Mbps)'].min():.2f} Mbps)")
    print(f"  Max:     {df['Throughput(MB/s)'].max():.2f} MB/s ({df['Throughput(Mbps)'].max():.2f} Mbps)")
    print(f"  Std Dev: {df['Throughput(MB/s)'].std():.2f} MB/s")
    print(f"\nTransfer Time Statistics:")
    print(f"  Average: {df['Time(s)'].mean():.3f} seconds")
    print(f"  Min:     {df['Time(s)'].min():.3f} seconds")
    print(f"  Max:     {df['Time(s)'].max():.3f} seconds")
    print("=" * 60)
    
    # Create visualizations
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('ADIOS2 WAN Transfer Performance (Utah → Clemson)', fontsize=16, fontweight='bold')
    
    # Plot 1: Throughput over time (MB/s)
    axes[0, 0].plot(df['Step'], df['Throughput(MB/s)'], 'b-o', linewidth=2, markersize=6)
    axes[0, 0].axhline(y=df['Throughput(MB/s)'].mean(), color='r', linestyle='--', 
                       label=f'Mean: {df["Throughput(MB/s)"].mean():.2f} MB/s')
    axes[0, 0].set_xlabel('Step Number', fontsize=11)
    axes[0, 0].set_ylabel('Throughput (MB/s)', fontsize=11)
    axes[0, 0].set_title('Throughput per Step (MB/s)', fontsize=12, fontweight='bold')
    axes[0, 0].grid(True, alpha=0.3)
    axes[0, 0].legend()
    
    # Plot 2: Throughput over time (Mbps)
    axes[0, 1].plot(df['Step'], df['Throughput(Mbps)'], 'g-o', linewidth=2, markersize=6)
    axes[0, 1].axhline(y=df['Throughput(Mbps)'].mean(), color='r', linestyle='--',
                       label=f'Mean: {df["Throughput(Mbps)"].mean():.2f} Mbps')
    axes[0, 1].set_xlabel('Step Number', fontsize=11)
    axes[0, 1].set_ylabel('Throughput (Mbps)', fontsize=11)
    axes[0, 1].set_title('Throughput per Step (Mbps)', fontsize=12, fontweight='bold')
    axes[0, 1].grid(True, alpha=0.3)
    axes[0, 1].legend()
    
    # Plot 3: Transfer time per step
    axes[1, 0].bar(df['Step'], df['Time(s)'], color='orange', alpha=0.7, edgecolor='black')
    axes[1, 0].axhline(y=df['Time(s)'].mean(), color='r', linestyle='--',
                       label=f'Mean: {df["Time(s)"].mean():.3f} s')
    axes[1, 0].set_xlabel('Step Number', fontsize=11)
    axes[1, 0].set_ylabel('Transfer Time (seconds)', fontsize=11)
    axes[1, 0].set_title('Transfer Time per Step', fontsize=12, fontweight='bold')
    axes[1, 0].grid(True, alpha=0.3, axis='y')
    axes[1, 0].legend()
    
    # Plot 4: Throughput distribution (histogram)
    axes[1, 1].hist(df['Throughput(MB/s)'], bins=15, color='purple', alpha=0.7, edgecolor='black')
    axes[1, 1].axvline(x=df['Throughput(MB/s)'].mean(), color='r', linestyle='--', linewidth=2,
                       label=f'Mean: {df["Throughput(MB/s)"].mean():.2f} MB/s')
    axes[1, 1].axvline(x=df['Throughput(MB/s)'].median(), color='g', linestyle='--', linewidth=2,
                       label=f'Median: {df["Throughput(MB/s)"].median():.2f} MB/s')
    axes[1, 1].set_xlabel('Throughput (MB/s)', fontsize=11)
    axes[1, 1].set_ylabel('Frequency', fontsize=11)
    axes[1, 1].set_title('Throughput Distribution', fontsize=12, fontweight='bold')
    axes[1, 1].grid(True, alpha=0.3, axis='y')
    axes[1, 1].legend()
    
    plt.tight_layout()
    
    # Save plot
    output_file = 'transfer_performance.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"\nPerformance plot saved to: {output_file}")
    
    # Show plot
    plt.show()

if __name__ == '__main__':
    csv_file = sys.argv[1] if len(sys.argv) > 1 else 'transfer_metrics.csv'
    analyze_metrics(csv_file)
