#!/usr/bin/env python3

import re
import sys
import matplotlib.pyplot as plt

# Constants
NUM_NODES = 3
BANDWIDTH_Gbps = 100  # Bandwidth in Gbps
BANDWIDTH_BYTES_PER_SEC = BANDWIDTH_Gbps * 1e9 / 8  # Convert Gbps to bytes per second

# Read input file path from arguments
if len(sys.argv) != 2:
    print("Usage: python script_name.py input_file_path")
    sys.exit(1)

input_file_path = sys.argv[1]

# Open the input file and read lines
with open(input_file_path, 'r') as f:
    lines = f.readlines()

# Initialize a list to store swapout events
swapout_events = []

# Regular expression pattern to match SWAPOUT lines
swapout_pattern = re.compile(
    r'(?P<time>\d+\.\d+)\s+SWAPOUT node:\s*(?P<node_id>\d+)\s+task:\s*(?P<task_id>\d+)\s+Bytes sent:\s*(?P<bytes_sent>\d+)\s+Computation time:\s*(?P<comp_time>\d+\.\d+)\s+Tasks remaining.*'
)

# Parse lines and extract SWAPOUT events
for line in lines:
    line = line.strip()
    match = swapout_pattern.match(line)
    if match:
        time = float(match.group('time'))
        node_id = int(match.group('node_id'))
        task_id = int(match.group('task_id'))
        bytes_sent = int(match.group('bytes_sent'))
        comp_time = float(match.group('comp_time'))
        swapout_events.append({
            'time': time,
            'node_id': node_id,
            'task_id': task_id,
            'bytes_sent': bytes_sent,
            'comp_time': comp_time
        })

# Sort the swapout events by time
swapout_events.sort(key=lambda x: x['time'])

# Process periods between swapouts
periods = []  # List of dicts with per-period data

for i, event in enumerate(swapout_events):
    if i == 0:
        start_time = 0
    else:
        start_time = swapout_events[i-1]['time']
    end_time = event['time']
    duration = end_time - start_time

    total_available_comp_time = duration * NUM_NODES  # total computation time available
    total_available_bandwidth = BANDWIDTH_BYTES_PER_SEC * duration  # total bandwidth in bytes

    total_actual_comp_time = event['comp_time']  # from 'Computation time' field
    total_actual_bytes_sent = event['bytes_sent']  # from 'Bytes sent' field

    comp_util = (total_actual_comp_time / total_available_comp_time) * 100 if total_available_comp_time > 0 else 0
    bandwidth_util = (total_actual_bytes_sent / total_available_bandwidth) * 100 if total_available_bandwidth > 0 else 0

    period_data = {
        'start_time': start_time,
        'end_time': end_time,
        'duration': duration,
        'total_available_comp_time': total_available_comp_time,
        'total_available_bandwidth': total_available_bandwidth,
        'total_actual_comp_time': total_actual_comp_time,
        'total_actual_bytes_sent': total_actual_bytes_sent,
        'comp_util': comp_util,
        'bandwidth_util': bandwidth_util
    }
    periods.append(period_data)

# Compute average utilities
total_duration = sum([p['duration'] for p in periods])
total_available_comp_time = sum([p['total_available_comp_time'] for p in periods])
total_available_bandwidth = sum([p['total_available_bandwidth'] for p in periods])
total_actual_comp_time = sum([p['total_actual_comp_time'] for p in periods])
total_actual_bytes_sent = sum([p['total_actual_bytes_sent'] for p in periods])

avg_comp_util = (total_actual_comp_time / total_available_comp_time) * 100 if total_available_comp_time > 0 else 0
avg_bandwidth_util = (total_actual_bytes_sent / total_available_bandwidth) * 100 if total_available_bandwidth > 0 else 0

# Extract utilities for plotting
comp_utils = [p['comp_util'] for p in periods]
bandwidth_utils = [p['bandwidth_util'] for p in periods]
period_labels = [f"{p['start_time']:.3f}-{p['end_time']:.3f}s" for p in periods]
indices = range(len(periods))

# Create subplots
fig, ax = plt.subplots()

# Plot computation utility
ax.plot(indices, comp_utils, marker='o', label='Computation Utility')

# Plot bandwidth utility
ax.plot(indices, bandwidth_utils, marker='x', label='Bandwidth Utility')

# Draw average utility lines
ax.axhline(y=avg_comp_util, color='blue', linestyle='--', label=f'Avg Computation Utility ({avg_comp_util:.2f}%)')
ax.axhline(y=avg_bandwidth_util, color='orange', linestyle='--', label=f'Avg Bandwidth Utility ({avg_bandwidth_util:.2f}%)')

# Set labels and title
ax.set_xlabel('Analysis Period')
ax.set_ylabel('Utility (%)')
ax.set_title('Computation and Bandwidth Utility per Period')
ax.set_xticks(indices)
ax.set_xticklabels(period_labels, rotation=45, ha='right')

# Add legend
ax.legend()

# Adjust layout to prevent clipping of tick-labels
fig.tight_layout()

# Save the figure
output_file = 'utility_graph.png'
plt.savefig(output_file)
print(f"Graph saved to {output_file}")
