import re
import numpy as np
import matplotlib.pyplot as plt
import sys
import csv
import os
import argparse
from scipy.fft import rfft, rfftfreq
from scipy.optimize import curve_fit

def parse_adc_windows(file_path):
    with open(file_path, "r") as f:
        log_text = f.read()

    matches = re.findall(r"START\s*(.*?)\s*END", log_text, re.S)
    windows = []
    for match in matches:
        nums_str = match.replace("\n", "")
        nums = [int(x.strip()) for x in nums_str.split(",") if x.strip().isdigit()]
        if len(nums) > 1 and any(n > 0 for n in nums):
            if nums[0] == 0 and nums[1] > 10:
                nums = nums[1:]
            windows.append(np.array(nums))
    return windows

# Sine function for fitting
def sine_func(t, A, f, phi):
    return A * np.sin(2 * np.pi * f * t + phi)

def analyze_signal(samples, fs, window_index, save=False, save_dir="adc_output", vref=3.3, nbits=12):
    N = len(samples)
    samples_v = (samples / (2**nbits - 1)) * vref
    samples_v = samples_v - np.mean(samples_v)  # remove DC

    # FFT
    freqs = rfftfreq(N, 1/fs)
    spectrum = np.abs(rfft(samples_v)) / (N/2)

    # Fundamental
    fundamental_idx = np.argmax(spectrum[1:]) + 1
    V1 = spectrum[fundamental_idx]
    f1 = freqs[fundamental_idx]

    # Harmonics 2–5
    harmonic_indices = []
    harmonics = []
    for k in range(2, 6):
        idx = fundamental_idx * k
        if idx < len(spectrum):
            harmonic_indices.append(idx)
            harmonics.append(spectrum[idx])
    harmonics = np.array(harmonics)

    # THD
    thd_linear = np.sqrt(np.sum(harmonics**2)) / V1
    THD_dB = 20 * np.log10(thd_linear) if thd_linear > 0 else -np.inf

    # --- FFT-based SNR (exclude fundamental and harmonics) ---
    excluded_bins = {0, fundamental_idx, *harmonic_indices}
    noise_bins = [spectrum[i] for i in range(len(spectrum)) if i not in excluded_bins]
    noise_rms = np.sqrt(np.sum(np.array(noise_bins)**2))
    SNR_dB = 20 * np.log10(V1 / noise_rms) if noise_rms > 0 else np.inf

    # --- Print results ---
    print(f"\n--- Window {window_index+1} ---")
    print(f"Samples: {N}, Fs: {fs/1e6:.3f} MHz")
    print(f"Fundamental frequency: {f1:.1f} Hz")
    print(f"THD: {THD_dB:.2f} dB")
    # print(f"SNR: {SNR_dB:.2f} dB")

    # --- Save stats ---
    if save:
        os.makedirs(save_dir, exist_ok=True)
        stats_path = os.path.join(save_dir, "adc_stats.csv")
        write_header = not os.path.exists(stats_path)
        with open(stats_path, "a", newline='') as csvfile:
            writer = csv.writer(csvfile)
            if write_header:
                writer.writerow(["Window", "Samples", "Fs (MHz)", "Fundamental (Hz)", "THD (dB)", "SNR (dB)"])
            writer.writerow([window_index+1, N, fs/1e6, f1, THD_dB, SNR_dB])

    # --- Time domain plot ---
    plt.figure()
    plt.plot(samples_v)
    plt.title(f"Captured waveform (V) - Window {window_index+1}")
    plt.xlabel("Sample index")
    plt.ylabel("Voltage [V]")
    plt.tight_layout()
    if save:
        plt.savefig(os.path.join(save_dir, f"waveform_window_{window_index+1}.png"))

    # --- FFT plot ---
    plt.figure()
    plt.semilogx(freqs[1:], 20*np.log10(spectrum[1:]))
    plt.title(f"FFT Spectrum - Window {window_index+1}")
    plt.title(f"FFT Spectrum - 100kHz signal")
    plt.xlabel("Frequency [Hz]")
    plt.ylabel("Magnitude [dB]")
    plt.grid(True, which="both", ls="--")
    plt.text(
        0.05, 0.95,
        f"THD: {THD_dB:.2f} dB",
        transform=plt.gca().transAxes,
        fontsize=9,
        verticalalignment='top',
        bbox=dict(facecolor='white', alpha=0.7)
    )
    plt.tight_layout()
    if save:
        plt.savefig(os.path.join(save_dir, f"fft_window_{window_index+1}.png"))

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Analyze ADC log file")
    parser.add_argument("file_path", type=str, help="Path to log file")
    parser.add_argument("fs", type=float, help="Sampling rate in Hz")
    parser.add_argument("--save", action="store_true", help="Save plots and stats")
    parser.add_argument("--batch_size", type=int, default=10, help="Number of windows per batch")
    args = parser.parse_args()

    windows = parse_adc_windows(args.file_path)
    for i, w in enumerate(windows):
        analyze_signal(w, args.fs, i, save=args.save)
        if (i + 1) % args.batch_size == 0 or (i + 1) == len(windows):
            plt.show()
            plt.close('all')
