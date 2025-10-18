%% Sampling frequency
x = x(:);
Fs = 1.159e6;  % Hz (from your example)

%% Remove DC offset
x = x - mean(x);

%% FFT
N = length(x);
X = fft(x);
X_mag = abs(X)/N;

%% Single-sided spectrum
X_mag = X_mag(1:floor(N/2)+1);
X_mag(2:end-1) = 2*X_mag(2:end-1);

f = (0:floor(N/2))*Fs/N;

%% Find fundamental frequency
[~, idx] = max(X_mag);
f0 = f(idx);
fprintf('Fundamental frequency: %.2f Hz\n', f0);

SNR_dB = snr(x, Fs);
fprintf('SNR (MATLAB snr function): %.2f dB\n', SNR_dB);

THD_dB = thd(x, Fs);
fprintf('THD: %.2f dB\n', THD_dB);

SINAD = sinad(x,Fs);
fprintf('SINAD: %.2f dB\n', SINAD);