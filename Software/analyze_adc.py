import re
import numpy as np
import matplotlib.pyplot as plt
import sys
from scipy.fft import rfft, rfftfreq

def parse_adc_window(file_path):
    with open(file_path, "r") as f:
        log_text = f.read()

    # Extract everything between '=== Triggered Window ===' and '=== End Window ==='
    match = re.search(r"=== Triggered Window \(raw values\) ===\s*(.*?)=== End Window ===", log_text, re.S)
    if not match:
        raise ValueError("Could not find triggered window in log.")
    
    nums_str = match.group(1).replace("\n", "")
    nums = [int(x.strip()) for x in nums_str.split(",") if x.strip().isdigit()]
    return np.array(nums)

def analyze_signal(file_path, fs, vref=3.3, nbits=12):
    samples = parse_adc_window(file_path)
    N = len(samples)

    # Convert to volts
    samples_v = (samples / (2**nbits - 1)) * vref
    samples_v = samples_v - np.mean(samples_v)  # remove DC

    # FFT
    freqs = rfftfreq(N, 1/fs)
    spectrum = np.abs(rfft(samples_v)) / (N/2)

    # Fundamental frequency = largest peak ignoring DC
    fundamental_idx = np.argmax(spectrum[1:]) + 1
    V1 = spectrum[fundamental_idx]
    f1 = freqs[fundamental_idx]

    # Harmonics (2nd to 5th)
    harmonics = []
    for k in range(2, 6):
        idx = fundamental_idx * k
        if idx < len(spectrum):
            harmonics.append(spectrum[idx])
    THD = np.sqrt(np.sum(np.array(harmonics)**2)) / V1

    # SINAD and ENOB
    excluded = {0, fundamental_idx, *[fundamental_idx * k for k in range(2,6) if fundamental_idx*k < len(spectrum)]}
    noise_bins = [spectrum[i] for i in range(len(spectrum)) if i not in excluded]
    noise_rms = np.sqrt(np.sum(np.array(noise_bins)**2))
    SINAD = 20*np.log10(V1 / np.sqrt(noise_rms**2 + np.sum(np.array(harmonics)**2)))
    ENOB = (SINAD - 1.76)/6.02

    # Jitter estimate (RMS of zero-crossing period variation)
    mean = np.mean(samples_v)
    crossings = np.where(np.diff(np.signbit(samples_v - mean)))[0]
    times = crossings / fs
    periods = np.diff(times[::2])
    jitter_ns = np.std(periods - np.mean(periods)) * 1e9

    # Print results
    print(f"Samples: {N}, Fs: {fs/1e6:.3f} MHz")
    print(f"Fundamental frequency: {f1:.1f} Hz")
    print(f"THD: {THD*100:.2f} %")
    print(f"SINAD: {SINAD:.2f} dB")
    print(f"ENOB: {ENOB:.2f} bits")
    print(f"Jitter RMS: {jitter_ns:.2f} ns")

    # Plots
    plt.figure()
    plt.plot(samples_v)
    plt.title("Captured waveform (volts)")
    plt.xlabel("Sample index")
    plt.ylabel("Voltage [V]")

    plt.figure()
    plt.semilogx(freqs[1:], 20*np.log10(spectrum[1:]))
    plt.title("FFT Spectrum")
    plt.xlabel("Frequency [Hz]")
    plt.ylabel("Magnitude [dB]")
    plt.grid(True, which="both", ls="--")
    plt.show()

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python analyze_adc.py <path_to_log.txt> <sampling_rate_Hz>")
        sys.exit(1)

    file_path = sys.argv[1]
    fs = float(sys.argv[2])
    analyze_signal(file_path, fs)
