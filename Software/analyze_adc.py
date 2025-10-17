import re
import numpy as np
import matplotlib.pyplot as plt
import sys
import csv
import os
import argparse
from scipy.fft import rfft, rfftfreq

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

def analyze_signal(samples, fs, window_index, save=False, save_dir="adc_output", vref=3.3, nbits=12):
    N = len(samples)
    samples_v = (samples / (2**nbits - 1)) * vref
    samples_v = samples_v - np.mean(samples_v)

    freqs = rfftfreq(N, 1/fs)
    spectrum = np.abs(rfft(samples_v)) / (N/2)

    fundamental_idx = np.argmax(spectrum[1:]) + 1
    V1 = spectrum[fundamental_idx]
    f1 = freqs[fundamental_idx]

    harmonics = []
    for k in range(2, 6):
        idx = fundamental_idx * k
        if idx < len(spectrum):
            harmonics.append(spectrum[idx])
    THD = np.sqrt(np.sum(np.array(harmonics)**2)) / V1

    excluded = {0, fundamental_idx, *[fundamental_idx * k for k in range(2,6) if fundamental_idx*k < len(spectrum)]}
    noise_bins = [spectrum[i] for i in range(len(spectrum)) if i not in excluded]
    noise_rms = np.sqrt(np.sum(np.array(noise_bins)**2))
    SINAD = 20*np.log10(V1 / np.sqrt(noise_rms**2 + np.sum(np.array(harmonics)**2)))
    ENOB = (SINAD - 1.76)/6.02
    THD_plus_N = np.sqrt(np.sum(np.array(harmonics)**2) + noise_rms**2) / V1

    mean = np.mean(samples_v)
    crossings = np.where(np.diff(np.signbit(samples_v - mean)))[0]
    times = crossings / fs
    periods = np.diff(times[::2])
    jitter_ns = np.std(periods - np.mean(periods)) * 1e9

    signal_rms = np.sqrt(np.mean(samples_v**2))
    noise = samples_v - np.mean(samples_v)
    noise_rms = np.sqrt(np.mean(noise**2))
    SNR_dB = 20 * np.log10(signal_rms / noise_rms)

    print(f"\n--- Window {window_index+1} ---")
    print(f"Samples: {N}, Fs: {fs/1e6:.3f} MHz")
    print(f"Fundamental frequency: {f1:.1f} Hz")
    print(f"THD: {THD*100:.2f} %")
    print(f"THD+N: {THD_plus_N*100:.2f} %")
    print(f"SNR: {SNR_dB:.2f} dB")  
    print(f"SINAD: {SINAD:.2f} dB")
    print(f"ENOB: {ENOB:.2f} bits")
    print(f"Jitter RMS: {jitter_ns:.2f} ns")

    if save:
        os.makedirs(save_dir, exist_ok=True)
        with open(os.path.join(save_dir, "adc_stats.csv"), "a", newline='') as csvfile:
            writer = csv.writer(csvfile)
            if window_index == 0:
                writer.writerow(["Window", "Samples", "Fs (MHz)", "Fundamental (Hz)", "THD (%)", "THD+N (%)", "SINAD (dB)", "ENOB (bits)", "Jitter (ns)"])
            writer.writerow([window_index+1, N, fs/1e6, f1, THD*100, THD_plus_N*100, SINAD, ENOB, jitter_ns])

    plt.figure()
    plt.plot(samples_v)
    plt.title(f"Captured waveform (volts) - Window {window_index+1}")
    plt.xlabel("Sample index")
    plt.ylabel("Voltage [V]")
    plt.tight_layout()
    if save:
        plt.savefig(os.path.join(save_dir, f"waveform_window_{window_index+1}.png"))

    plt.figure()
    plt.semilogx(freqs[1:], 20*np.log10(spectrum[1:]))
    plt.title(f"FFT Spectrum - Window {window_index+1}")
    plt.xlabel("Frequency [Hz]")
    plt.ylabel("Magnitude [dB]")
    plt.grid(True, which="both", ls="--")
    plt.text(0.05, 0.95,
             f"THD: {THD*100:.2f}%\nTHD+N: {THD_plus_N*100:.2f}%\nSINAD: {SINAD:.2f} dB\nENOB: {ENOB:.2f} bits\nJitter: {jitter_ns:.2f} ns",
             transform=plt.gca().transAxes,
             fontsize=9,
             verticalalignment='top',
             bbox=dict(facecolor='white', alpha=0.7))
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
    for i in range(len(windows)):
        analyze_signal(windows[i], args.fs, i, save=args.save)
        if (i + 1) % args.batch_size == 0 or (i + 1) == len(windows):
            plt.show()
            plt.close('all')


# import re
# import numpy as np
# import matplotlib.pyplot as plt
# import sys
# import csv
# import os
# from scipy.fft import rfft, rfftfreq

# def parse_adc_windows(file_path):
#     with open(file_path, "r") as f:
#         log_text = f.read()

#     matches = re.findall(r"START\s*(.*?)\s*END", log_text, re.S)
#     windows = []
#     for match in matches:
#         nums_str = match.replace("\n", "")
#         nums = [int(x.strip()) for x in nums_str.split(",") if x.strip().isdigit()]
#         if len(nums) > 1 and any(n > 0 for n in nums):
#             if nums[0] == 0 and nums[1] > 10:
#                 nums = nums[1:]
#             windows.append(np.array(nums))
#     return windows

# def analyze_signal(samples, fs, window_index, save=False, save_dir="adc_output", vref=3.3, nbits=12):
#     N = len(samples)
#     samples_v = (samples / (2**nbits - 1)) * vref
#     samples_v = samples_v - np.mean(samples_v)

#     freqs = rfftfreq(N, 1/fs)
#     spectrum = np.abs(rfft(samples_v)) / (N/2)

#     fundamental_idx = np.argmax(spectrum[1:]) + 1
#     V1 = spectrum[fundamental_idx]
#     f1 = freqs[fundamental_idx]

#     harmonics = []
#     for k in range(2, 6):
#         idx = fundamental_idx * k
#         if idx < len(spectrum):
#             harmonics.append(spectrum[idx])
#     THD = np.sqrt(np.sum(np.array(harmonics)**2)) / V1

#     excluded = {0, fundamental_idx, *[fundamental_idx * k for k in range(2,6) if fundamental_idx*k < len(spectrum)]}
#     noise_bins = [spectrum[i] for i in range(len(spectrum)) if i not in excluded]
#     noise_rms = np.sqrt(np.sum(np.array(noise_bins)**2))
#     SINAD = 20*np.log10(V1 / np.sqrt(noise_rms**2 + np.sum(np.array(harmonics)**2)))
#     ENOB = (SINAD - 1.76)/6.02
#     THD_plus_N = np.sqrt(np.sum(np.array(harmonics)**2) + noise_rms**2) / V1

#     mean = np.mean(samples_v)
#     crossings = np.where(np.diff(np.signbit(samples_v - mean)))[0]
#     times = crossings / fs
#     periods = np.diff(times[::2])
#     jitter_ns = np.std(periods - np.mean(periods)) * 1e9

#     print(f"\n--- Window {window_index+1} ---")
#     print(f"Samples: {N}, Fs: {fs/1e6:.3f} MHz")
#     print(f"Fundamental frequency: {f1:.1f} Hz")
#     print(f"THD: {THD*100:.2f} %")
#     print(f"THD+N: {THD_plus_N*100:.2f} %")
#     print(f"SINAD: {SINAD:.2f} dB")
#     print(f"ENOB: {ENOB:.2f} bits")
#     print(f"Jitter RMS: {jitter_ns:.2f} ns")

#     if save:
#         os.makedirs(save_dir, exist_ok=True)
#         with open(os.path.join(save_dir, "adc_stats.csv"), "a", newline='') as csvfile:
#             writer = csv.writer(csvfile)
#             if window_index == 0:
#                 writer.writerow(["Window", "Samples", "Fs (MHz)", "Fundamental (Hz)", "THD (%)", "THD+N (%)", "SINAD (dB)", "ENOB (bits)", "Jitter (ns)"])
#             writer.writerow([window_index+1, N, fs/1e6, f1, THD*100, THD_plus_N*100, SINAD, ENOB, jitter_ns])

#     plt.figure()
#     plt.plot(samples_v)
#     plt.title(f"Captured waveform (volts) - Window {window_index+1}")
#     plt.xlabel("Sample index")
#     plt.ylabel("Voltage [V]")
#     plt.tight_layout()
#     if save:
#         plt.savefig(os.path.join(save_dir, f"waveform_window_{window_index+1}.png"))

#     plt.figure()
#     plt.semilogx(freqs[1:], 20*np.log10(spectrum[1:]))
#     plt.title(f"FFT Spectrum - Window {window_index+1}")
#     plt.xlabel("Frequency [Hz]")
#     plt.ylabel("Magnitude [dB]")
#     plt.grid(True, which="both", ls="--")
#     plt.text(0.05, 0.95,
#              f"THD: {THD*100:.2f}%\nTHD+N: {THD_plus_N*100:.2f}%\nSINAD: {SINAD:.2f} dB\nENOB: {ENOB:.2f} bits\nJitter: {jitter_ns:.2f} ns",
#              transform=plt.gca().transAxes,
#              fontsize=9,
#              verticalalignment='top',
#              bbox=dict(facecolor='white', alpha=0.7))
#     plt.tight_layout()
#     if save:
#         plt.savefig(os.path.join(save_dir, f"fft_window_{window_index+1}.png"))

# if __name__ == "__main__":
#     if len(sys.argv) < 3:
#         print("Usage: python analyze_adc.py <path_to_log.txt> <sampling_rate_Hz> [--save]")
#         sys.exit(1)

#     file_path = sys.argv[1]
#     fs = float(sys.argv[2])
#     save_flag = '--save' in sys.argv

#     windows = parse_adc_windows(file_path)
#     max_windows = 10
#     for i in range(len(windows)):
#         if i > 0 and i % max_windows == 0:
#             plt.close('all')
#         analyze_signal(windows[i], fs, i, save=save_flag)

#     plt.show()


# import re
# import numpy as np
# import matplotlib.pyplot as plt
# import sys
# from scipy.fft import rfft, rfftfreq

# def parse_adc_windows(file_path):
#     with open(file_path, "r") as f:
#         log_text = f.read()

#     # Extract all windows between 'START' and 'END'
#     matches = re.findall(r"START\s*(.*?)\s*END", log_text, re.S)
#     if not matches:
#         raise ValueError("Could not find any triggered windows in log.")

#     windows = []
#     for match in matches:
#         nums_str = match.replace("\n", "")
#         nums = [int(x.strip()) for x in nums_str.split(",") if x.strip().isdigit()]
#         windows.append(np.array(nums))
#     return windows

# def analyze_signal(samples, fs, window_index, vref=3.3, nbits=12):
#     N = len(samples)

#     # Convert to volts
#     samples_v = (samples / (2**nbits - 1)) * vref
#     samples_v = samples_v - np.mean(samples_v)  # remove DC

#     # FFT
#     freqs = rfftfreq(N, 1/fs)
#     spectrum = np.abs(rfft(samples_v)) / (N/2)

#     # Fundamental frequency
#     fundamental_idx = np.argmax(spectrum[1:]) + 1
#     V1 = spectrum[fundamental_idx]
#     f1 = freqs[fundamental_idx]

#     # Harmonics
#     harmonics = []
#     for k in range(2, 6):
#         idx = fundamental_idx * k
#         if idx < len(spectrum):
#             harmonics.append(spectrum[idx])
#     THD = np.sqrt(np.sum(np.array(harmonics)**2)) / V1

#     # SINAD and ENOB
#     excluded = {0, fundamental_idx, *[fundamental_idx * k for k in range(2,6) if fundamental_idx*k < len(spectrum)]}
#     noise_bins = [spectrum[i] for i in range(len(spectrum)) if i not in excluded]
#     noise_rms = np.sqrt(np.sum(np.array(noise_bins)**2))
#     SINAD = 20*np.log10(V1 / np.sqrt(noise_rms**2 + np.sum(np.array(harmonics)**2)))
#     ENOB = (SINAD - 1.76)/6.02

#     # Jitter estimate
#     mean = np.mean(samples_v)
#     crossings = np.where(np.diff(np.signbit(samples_v - mean)))[0]
#     times = crossings / fs
#     periods = np.diff(times[::2])
#     jitter_ns = np.std(periods - np.mean(periods)) * 1e9

#     # Print results
#     print(f"\n--- Window {window_index+1} ---")
#     print(f"Samples: {N}, Fs: {fs/1e6:.3f} MHz")
#     print(f"Fundamental frequency: {f1:.1f} Hz")
#     print(f"THD: {THD*100:.2f} %")
#     print(f"SINAD: {SINAD:.2f} dB")
#     print(f"ENOB: {ENOB:.2f} bits")
#     print(f"Jitter RMS: {jitter_ns:.2f} ns")

#     # Plots
#     plt.figure()
#     plt.plot(samples_v)
#     plt.title(f"Captured waveform (volts) - Window {window_index+1}")
#     plt.xlabel("Sample index")
#     plt.ylabel("Voltage [V]")

#     plt.figure()
#     plt.semilogx(freqs[1:], 20*np.log10(spectrum[1:]))
#     plt.title(f"FFT Spectrum - Window {window_index+1}")
#     plt.xlabel("Frequency [Hz]")
#     plt.ylabel("Magnitude [dB]")
#     plt.grid(True, which="both", ls="--")

# if __name__ == "__main__":
#     if len(sys.argv) != 3:
#         print("Usage: python analyze_adc.py <path_to_log.txt> <sampling_rate_Hz>")
#         sys.exit(1)

#     file_path = sys.argv[1]
#     fs = float(sys.argv[2])
#     windows = parse_adc_windows(file_path)

#     for i, samples in enumerate(windows):
#         analyze_signal(samples, fs, i)

#     plt.show()



# # import re
# # import numpy as np
# # import matplotlib.pyplot as plt
# # import sys
# # from scipy.fft import rfft, rfftfreq

# # def parse_adc_window(file_path):
# #     with open(file_path, "r") as f:
# #         log_text = f.read()

# #     # Extract everything between '=== Triggered Window ===' and '=== End Window ==='
# #     match = re.search(r"START*(.*?)END", log_text, re.S)
# #     if not match:
# #         raise ValueError("Could not find triggered window in log.")
    
# #     nums_str = match.group(1).replace("\n", "")
# #     nums = [int(x.strip()) for x in nums_str.split(",") if x.strip().isdigit()]
# #     return np.array(nums)

# # def analyze_signal(file_path, fs, vref=3.3, nbits=12):
# #     samples = parse_adc_window(file_path)
# #     N = len(samples)

# #     # Convert to volts
# #     samples_v = (samples / (2**nbits - 1)) * vref
# #     samples_v = samples_v - np.mean(samples_v)  # remove DC

# #     # FFT
# #     freqs = rfftfreq(N, 1/fs)
# #     spectrum = np.abs(rfft(samples_v)) / (N/2)

# #     # Fundamental frequency = largest peak ignoring DC
# #     fundamental_idx = np.argmax(spectrum[1:]) + 1
# #     V1 = spectrum[fundamental_idx]
# #     f1 = freqs[fundamental_idx]

# #     # Harmonics (2nd to 5th)
# #     harmonics = []
# #     for k in range(2, 6):
# #         idx = fundamental_idx * k
# #         if idx < len(spectrum):
# #             harmonics.append(spectrum[idx])
# #     THD = np.sqrt(np.sum(np.array(harmonics)**2)) / V1

# #     # SINAD and ENOB
# #     excluded = {0, fundamental_idx, *[fundamental_idx * k for k in range(2,6) if fundamental_idx*k < len(spectrum)]}
# #     noise_bins = [spectrum[i] for i in range(len(spectrum)) if i not in excluded]
# #     noise_rms = np.sqrt(np.sum(np.array(noise_bins)**2))
# #     SINAD = 20*np.log10(V1 / np.sqrt(noise_rms**2 + np.sum(np.array(harmonics)**2)))
# #     ENOB = (SINAD - 1.76)/6.02

# #     # Jitter estimate (RMS of zero-crossing period variation)
# #     mean = np.mean(samples_v)
# #     crossings = np.where(np.diff(np.signbit(samples_v - mean)))[0]
# #     times = crossings / fs
# #     periods = np.diff(times[::2])
# #     jitter_ns = np.std(periods - np.mean(periods)) * 1e9

# #     # Print results
# #     print(f"Samples: {N}, Fs: {fs/1e6:.3f} MHz")
# #     print(f"Fundamental frequency: {f1:.1f} Hz")
# #     print(f"THD: {THD*100:.2f} %")
# #     print(f"SINAD: {SINAD:.2f} dB")
# #     print(f"ENOB: {ENOB:.2f} bits")
# #     print(f"Jitter RMS: {jitter_ns:.2f} ns")

# #     # Plots
# #     plt.figure()
# #     plt.plot(samples_v)
# #     plt.title("Captured waveform (volts)")
# #     plt.xlabel("Sample index")
# #     plt.ylabel("Voltage [V]")

# #     plt.figure()
# #     plt.semilogx(freqs[1:], 20*np.log10(spectrum[1:]))
# #     plt.title("FFT Spectrum")
# #     plt.xlabel("Frequency [Hz]")
# #     plt.ylabel("Magnitude [dB]")
# #     plt.grid(True, which="both", ls="--")
# #     plt.show()

# # if __name__ == "__main__":
# #     if len(sys.argv) != 3:
# #         print("Usage: python analyze_adc.py <path_to_log.txt> <sampling_rate_Hz>")
# #         sys.exit(1)

# #     file_path = sys.argv[1]
# #     fs = float(sys.argv[2])
# #     analyze_signal(file_path, fs)
