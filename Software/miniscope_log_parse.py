import re
import sys
import matplotlib.pyplot as plt
import numpy as np

# Check for argument
if len(sys.argv) < 2:
    print("Usage: python plot_miniscope.py <log_file>")
    sys.exit(1)

log_file = sys.argv[1]

# Read the file
try:
    with open(log_file, "r") as f:
        log_data = f.read()
except FileNotFoundError:
    print(f"Error: File '{log_file}' not found.")
    sys.exit(1)

# Find all raw values blocks
matches = re.findall(r"=== Triggered Window \(raw values\) ===\s*(.*?)\s*=== End Window ===", log_data, re.DOTALL)

if not matches:
    print("No triggered windows found in the file.")
    sys.exit(1)

# ADC properties
ADC_BITS = 12
MAX_ADC_VALUE = 2**ADC_BITS - 1
VOLTAGE_RANGE = 3.3  # Volts

# Function to convert ADC value to voltage
def adc_to_voltage(adc_value):
    return (adc_value / MAX_ADC_VALUE) * VOLTAGE_RANGE

# Process and plot each triggered window
for i, match_content in enumerate(matches):
    # Parse values
    raw_values = []
    for val in match_content.split(','):
        stripped_val = val.strip()
        if stripped_val.isdigit():
            raw_values.append(int(stripped_val))

    if not raw_values:
        print(f"Warning: No valid raw values found in triggered window {i+1}. Skipping plot.")
        continue

    # Convert to numpy array for easier calculations
    raw_values_np = np.array(raw_values)

    # Calculate peak and low ADC values
    peak_adc = np.max(raw_values_np)
    low_adc = np.min(raw_values_np)

    # Convert to voltage
    peak_voltage = adc_to_voltage(peak_adc)
    low_voltage = adc_to_voltage(low_adc)

    # Plot
    plt.figure(figsize=(12, 6))
    plt.plot(raw_values_np, lw=0.8, marker='o', markersize=3)
    plt.title(f"MiniScope Triggered Window {i+1} (Raw Values)")
    plt.xlabel("Sample Index")
    plt.ylabel("ADC Value")
    plt.grid(True)

    # Display peak and low voltages
    plt.text(0.02, 0.98, f'Peak Voltage: {peak_voltage:.3f}V (ADC: {peak_adc})',
             transform=plt.gca().transAxes, verticalalignment='top',
             bbox=dict(boxstyle="round,pad=0.3", fc="yellow", ec="b", lw=0.5, alpha=0.5))
    plt.text(0.02, 0.90, f'Low Voltage: {low_voltage:.3f}V (ADC: {low_adc})',
             transform=plt.gca().transAxes, verticalalignment='top',
             bbox=dict(boxstyle="round,pad=0.3", fc="lightcoral", ec="b", lw=0.5, alpha=0.5))

    # Attempt to estimate signal frequency, width, and period
    # This is a basic attempt and might not be accurate for all signal types (e.g., pulses, noisy data)
    # For more robust analysis, consider signal processing libraries.

    # Find peaks for period/frequency estimation
    # A simple approach: consider points above a certain threshold relative to min/max
    threshold = low_adc + (peak_adc - low_adc) * 0.5 # Mid-point threshold
    
    # Find indices where the signal crosses the threshold going up
    crossings_up = np.where(np.diff(raw_values_np > threshold) == 1)[0] + 1
    
    # Estimate period from average distance between consecutive rising edges
    if len(crossings_up) > 1:
        sample_period = np.mean(np.diff(crossings_up))
        
        # Assuming sample rate is unknown, we can only provide period in samples
        plt.text(0.02, 0.82, f'Estimated Period: {sample_period:.2f} samples',
                 transform=plt.gca().transAxes, verticalalignment='top',
                 bbox=dict(boxstyle="round,pad=0.3", fc="lightgreen", ec="b", lw=0.5, alpha=0.5))
        
        # If we had a sample rate (e.g., 1000 samples/second), we could calculate frequency:
        # sample_rate = 1000 # samples/second (example, replace with actual)
        # frequency = sample_rate / sample_period
        # plt.text(0.02, 0.74, f'Estimated Frequency: {frequency:.2f} Hz', transform=plt.gca().transAxes, verticalalignment='top')
    else:
        pass
        # plt.text(0.02, 0.82, 'Period/Frequency: Not enough cycles detected',
        #          transform=plt.gca().transAxes, verticalalignment='top',
        #          bbox=dict(boxstyle="round,pad=0.3", fc="lightgrey", ec="b", lw=0.5, alpha=0.5))

    # Basic estimation of pulse width (might not be accurate for complex waveforms)
    # For a simple pulse, width could be the number of samples above a high threshold
    high_threshold = low_adc + (peak_adc - low_adc) * 0.8 # 80% of full range
    pulse_samples = np.where(raw_values_np > high_threshold)[0]
    
    if len(pulse_samples) > 0:
        # A very simplified width estimation: count consecutive samples above threshold
        # This doesn't distinguish between -width and +width easily for oscillating signals
        # For a true pulse, this might be its duration.
        
        # To get more accurate +width and -width, you'd typically need to define a baseline
        # and then look at the duration above/below that baseline for positive/negative going pulses.
        # Given the provided data is raw ADC values, precise +width/-width requires more context
        # about what constitutes a "pulse" in your signal.

        # For demonstration, let's consider the number of samples significantly above average
        avg_val = np.mean(raw_values_np)
        
        # Simple positive width: samples above average + some delta
        pos_width_samples = np.sum(raw_values_np > (avg_val + (peak_adc - avg_val) * 0.5)) # Above 50% of the positive swing
        
        # Simple negative width: samples below average - some delta
        neg_width_samples = np.sum(raw_values_np < (avg_val - (avg_val - low_adc) * 0.5)) # Below 50% of the negative swing

    #     if pos_width_samples > 0:
    #         plt.text(0.02, 0.74, f'Estimated +Width: {pos_width_samples} samples',
    #                  transform=plt.gca().transAxes, verticalalignment='top',
    #                  bbox=dict(boxstyle="round,pad=0.3", fc="lightblue", ec="b", lw=0.5, alpha=0.5))
    #     if neg_width_samples > 0:
    #         plt.text(0.02, 0.66, f'Estimated -Width: {neg_width_samples} samples',
    #                  transform=plt.gca().transAxes, verticalalignment='top',
    #                  bbox=dict(boxstyle="round,pad=0.3", fc="pink", ec="b", lw=0.5, alpha=0.5))
    # else:
    #     plt.text(0.02, 0.74, 'Width: Not clearly detectable (flat or noisy)',
    #              transform=plt.gca().transAxes, verticalalignment='top',
    #              bbox=dict(boxstyle="round,pad=0.3", fc="lightgrey", ec="b", lw=0.5, alpha=0.5))

    plt.tight_layout()
    plt.show()
